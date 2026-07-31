// VoxelDebugVisualizer.cpp

#include "VoxelDebugVisualizer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelChunk.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBiomeDefinition.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelDebug, Log, All);

AVoxelDebugVisualizer::AVoxelDebugVisualizer()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		CubeMesh = CubeMeshFinder.Object;
	}
}

UInstancedStaticMeshComponent* AVoxelDebugVisualizer::GetOrCreateComponentForBlock(int32 BlockId)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = BlockIdToComponent.Find(BlockId))
	{
		return *Existing;
	}

	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("BlockISMC_%d"), BlockId));
	Component->SetStaticMesh(CubeMesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); // debug-only, not a real collision source
	Component->SetupAttachment(RootComponent);
	Component->RegisterComponent();

	UMaterialInterface* Material = nullptr;
	if (TObjectPtr<UMaterialInterface>* Override = BlockMaterials.Find(BlockId))
	{
		Material = *Override;
	}
	else
	{
		Material = DefaultMaterial;
	}

	if (Material)
	{
		Component->SetMaterial(0, Material);
	}

	BlockIdToComponent.Add(BlockId, Component);
	return Component;
}

void AVoxelDebugVisualizer::ClearVisualization()
{
	for (const TPair<int32, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : BlockIdToComponent)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	BlockIdToComponent.Reset();
}

void AVoxelDebugVisualizer::GenerateAndVisualize()
{
	ClearVisualization();

	if (!CubeMesh)
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("CubeMesh could not be loaded (/Engine/BasicShapes/Cube.Cube) - cannot visualize."));
		return;
	}

	// Build a local, non-subsystem block registry purely to resolve biome
	// terrain layers for this preview - see VoxelBlockRegistry.h, this must
	// happen on the Game Thread, which GenerateAndVisualize already is.
	UVoxelBlockRegistry* LocalRegistry = nullptr;
	TArray<const UVoxelBiomeDefinition*> AvailableBiomes;
	if (Biomes.Num() > 0)
	{
		LocalRegistry = NewObject<UVoxelBlockRegistry>(this);
		TArray<UVoxelBiomeDefinition*> BiomePtrs;
		for (const TObjectPtr<UVoxelBiomeDefinition>& Biome : Biomes)
		{
			if (Biome)
			{
				BiomePtrs.Add(Biome);
				AvailableBiomes.Add(Biome);
			}
		}
		LocalRegistry->PrecacheBiomeLayers(BiomePtrs);
	}

	FVoxelGenerationPipeline Pipeline;

	const int32 HalfRadius = ChunkRadiusXY;
	TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> GeneratedChunks;

	const double GenStart = FPlatformTime::Seconds();

	for (int32 CX = -HalfRadius; CX < HalfRadius; ++CX)
	{
		for (int32 CY = -HalfRadius; CY < HalfRadius; ++CY)
		{
			for (int32 CZ = 0; CZ < ChunkCountZ; ++CZ)
			{
				const FVoxelChunkCoordinate Coord(CX, CY, CZ);
				TUniquePtr<FVoxelChunk> Chunk = MakeUnique<FVoxelChunk>(ChunkSize);
				Pipeline.GenerateChunk(WorldSeed, Coord, ChunkSize, LocalRegistry, AvailableBiomes, *Chunk);
				GeneratedChunks.Add(Coord, MoveTemp(Chunk));
			}
		}
	}

	const double GenElapsedMs = (FPlatformTime::Seconds() - GenStart) * 1000.0;
	UE_LOG(LogVoxelDebug, Log, TEXT("Generated %d chunks in %.2f ms."), GeneratedChunks.Num(), GenElapsedMs);

	// World-space voxel lookup across the whole generated region, treating
	// anything outside it (ungenerated chunks) as air - this means edge
	// chunks show their outward faces, which is the expected/correct look
	// for a bounded debug preview.
	auto GetGlobalBlock = [&](int32 WorldX, int32 WorldY, int32 WorldZ) -> FVoxelBlockId
	{
		const int32 CX = FMath::FloorToInt(static_cast<float>(WorldX) / ChunkSize);
		const int32 CY = FMath::FloorToInt(static_cast<float>(WorldY) / ChunkSize);
		const int32 CZ = FMath::FloorToInt(static_cast<float>(WorldZ) / ChunkSize);

		const TUniquePtr<FVoxelChunk>* Found = GeneratedChunks.Find(FVoxelChunkCoordinate(CX, CY, CZ));
		if (!Found)
		{
			return VoxelBlockId_Air;
		}

		const int32 LocalX = WorldX - CX * ChunkSize;
		const int32 LocalY = WorldY - CY * ChunkSize;
		const int32 LocalZ = WorldZ - CZ * ChunkSize;
		return (*Found)->GetBlock(LocalX, LocalY, LocalZ);
	};

	int32 TotalSolidVoxels = 0;
	int32 TotalVisibleVoxels = 0;

	for (const TPair<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>>& Entry : GeneratedChunks)
	{
		const FVoxelChunkCoordinate& Coord = Entry.Key;
		const FVoxelChunk& Chunk = *Entry.Value;
		const int32 BaseX = Coord.X * ChunkSize;
		const int32 BaseY = Coord.Y * ChunkSize;
		const int32 BaseZ = Coord.Z * ChunkSize;

		for (int32 LocalZ = 0; LocalZ < ChunkSize; ++LocalZ)
		{
			for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
			{
				for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
				{
					const FVoxelBlockId BlockId = Chunk.GetBlock(LocalX, LocalY, LocalZ);
					if (BlockId == VoxelBlockId_Air)
					{
						continue;
					}
					++TotalSolidVoxels;

					const int32 WorldX = BaseX + LocalX;
					const int32 WorldY = BaseY + LocalY;
					const int32 WorldZ = BaseZ + LocalZ;

					// Cheap culling: skip voxels fully surrounded by solid
					// neighbors on all 6 sides - not real face culling
					// (still one cube per exposed voxel, not per face), but
					// enough to keep instance counts sane for a debug tool.
					const bool bExposed =
						GetGlobalBlock(WorldX + 1, WorldY, WorldZ) == VoxelBlockId_Air ||
						GetGlobalBlock(WorldX - 1, WorldY, WorldZ) == VoxelBlockId_Air ||
						GetGlobalBlock(WorldX, WorldY + 1, WorldZ) == VoxelBlockId_Air ||
						GetGlobalBlock(WorldX, WorldY - 1, WorldZ) == VoxelBlockId_Air ||
						GetGlobalBlock(WorldX, WorldY, WorldZ + 1) == VoxelBlockId_Air ||
						GetGlobalBlock(WorldX, WorldY, WorldZ - 1) == VoxelBlockId_Air;

					if (!bExposed)
					{
						continue;
					}
					++TotalVisibleVoxels;

					UInstancedStaticMeshComponent* Component = GetOrCreateComponentForBlock(BlockId);

					const FVector Location(WorldX * VoxelWorldSize, WorldY * VoxelWorldSize, WorldZ * VoxelWorldSize);
					const float Scale = VoxelWorldSize / 100.0f; // cube mesh is 100uu native
					FTransform InstanceTransform(FRotator::ZeroRotator, Location, FVector(Scale));
					Component->AddInstance(InstanceTransform);
				}
			}
		}
	}

	UE_LOG(LogVoxelDebug, Log, TEXT("Visualized %d/%d solid voxels (%d culled as fully buried) across %d block-ID components."),
		TotalVisibleVoxels, TotalSolidVoxels, TotalSolidVoxels - TotalVisibleVoxels, BlockIdToComponent.Num());
}
