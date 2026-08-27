// VoxelDebugVisualizer.cpp

#include "VoxelDebugVisualizer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelChunk.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBiomeDefinition.h"
#include "VoxelMesher.h"
#include "VoxelMeshData.h"
#include "VoxelMeshComponent.h"
#include "VoxelWorldSubsystem.h"
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

	for (const TObjectPtr<UProceduralMeshComponent>& Component : MeshPreviewComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	MeshPreviewComponents.Reset();

	for (const TObjectPtr<UVoxelMeshComponent>& Component : RenderedPreviewComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	RenderedPreviewComponents.Reset();
}

TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> AVoxelDebugVisualizer::GenerateChunkGrid(UVoxelBlockRegistry*& OutRegistry)
{
	OutRegistry = nullptr;
	TArray<const UVoxelBiomeDefinition*> AvailableBiomes;

	if (Biomes.Num() > 0)
	{
		OutRegistry = NewObject<UVoxelBlockRegistry>(this);
		TArray<UVoxelBiomeDefinition*> BiomePtrs;
		for (const TObjectPtr<UVoxelBiomeDefinition>& Biome : Biomes)
		{
			if (Biome)
			{
				BiomePtrs.Add(Biome);
				AvailableBiomes.Add(Biome);
			}
		}
		OutRegistry->PrecacheBiomeLayers(BiomePtrs);
	}

	FVoxelGenerationPipeline Pipeline;
	TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> GeneratedChunks;

	const double GenStart = FPlatformTime::Seconds();
	const int32 HalfRadius = ChunkRadiusXY;

	for (int32 CX = -HalfRadius; CX < HalfRadius; ++CX)
	{
		for (int32 CY = -HalfRadius; CY < HalfRadius; ++CY)
		{
			for (int32 CZ = 0; CZ < ChunkCountZ; ++CZ)
			{
				const FVoxelChunkCoordinate Coord(CX, CY, CZ);
				TUniquePtr<FVoxelChunk> Chunk = MakeUnique<FVoxelChunk>(ChunkSize);
				Pipeline.GenerateChunk(WorldSeed, Coord, ChunkSize, OutRegistry, AvailableBiomes, *Chunk);
				GeneratedChunks.Add(Coord, MoveTemp(Chunk));
			}
		}
	}

	const double GenElapsedMs = (FPlatformTime::Seconds() - GenStart) * 1000.0;
	UE_LOG(LogVoxelDebug, Log, TEXT("Generated %d chunks in %.2f ms."), GeneratedChunks.Num(), GenElapsedMs);

	return GeneratedChunks;
}

void AVoxelDebugVisualizer::GenerateAndVisualize()
{
	ClearVisualization();

	if (!CubeMesh)
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("CubeMesh could not be loaded (/Engine/BasicShapes/Cube.Cube) - cannot visualize."));
		return;
	}

	UVoxelBlockRegistry* LocalRegistry = nullptr;
	TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> GeneratedChunks = GenerateChunkGrid(LocalRegistry);

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
					const float Scale = VoxelWorldSize / 100.0f;
					FTransform InstanceTransform(FRotator::ZeroRotator, Location, FVector(Scale));
					Component->AddInstance(InstanceTransform);
				}
			}
		}
	}

	UE_LOG(LogVoxelDebug, Log, TEXT("[Cube preview] Visualized %d/%d solid voxels (%d culled as fully buried) across %d block-ID components."),
		TotalVisibleVoxels, TotalSolidVoxels, TotalSolidVoxels - TotalVisibleVoxels, BlockIdToComponent.Num());
}

void AVoxelDebugVisualizer::GenerateAndVisualizeMeshed()
{
	ClearVisualization();

	UVoxelBlockRegistry* LocalRegistry = nullptr;
	TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> GeneratedChunks = GenerateChunkGrid(LocalRegistry);

	int32 TotalVertices = 0;
	int32 TotalTriangles = 0;
	double TotalMeshingMs = 0.0;

	// One PMC per chunk - simplest correct approach for a debug tool. A
	// real world subsystem would likely want fewer, larger draw calls, but
	// that's a VoxelRendering concern, not something to optimize here.
	for (const TPair<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>>& Entry : GeneratedChunks)
	{
		const FVoxelChunkCoordinate& Coord = Entry.Key;
		const FVoxelChunk& Chunk = *Entry.Value;

		const double MeshStart = FPlatformTime::Seconds();
		const FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, LocalRegistry);
		TotalMeshingMs += (FPlatformTime::Seconds() - MeshStart) * 1000.0;

		if (MeshData.IsEmpty())
		{
			continue; // fully-air chunk, nothing to show
		}

		UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this,
			*FString::Printf(TEXT("MeshPreview_%d_%d_%d"), Coord.X, Coord.Y, Coord.Z));
		PMC->SetMobility(EComponentMobility::Movable);
		PMC->SetupAttachment(RootComponent);
		PMC->RegisterComponent();
		PMC->bUseComplexAsSimpleCollision = true;

		// Shared per-chunk conversion buffers, rebuilt per section since
		// FVoxelMeshData::Vertices is one shared array indexed by each
		// section's own index list - PMC wants one contiguous vertex/index
		// buffer per section, so we remap indices into a compact per-section range.
		const FVector ChunkWorldOrigin(
			Coord.X * ChunkSize * VoxelWorldSize,
			Coord.Y * ChunkSize * VoxelWorldSize,
			Coord.Z * ChunkSize * VoxelWorldSize);

		for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
		{
			const FVoxelMeshSection& Section = MeshData.Sections[SectionIndex];
			if (Section.Indices.Num() == 0)
			{
				continue;
			}

			TArray<FVector> Positions;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FColor> VertexColors;
			TArray<int32> Triangles;
			TArray<FProcMeshTangent> Tangents; // left empty - fine for a debug preview, PMC handles it gracefully

			TMap<uint32, int32> GlobalToLocalVertexIndex;
			Positions.Reserve(Section.Indices.Num());
			Normals.Reserve(Section.Indices.Num());
			UVs.Reserve(Section.Indices.Num());
			VertexColors.Reserve(Section.Indices.Num());
			Triangles.Reserve(Section.Indices.Num());

			for (uint32 GlobalIndex : Section.Indices)
			{
				int32 LocalIndex;
				if (const int32* Existing = GlobalToLocalVertexIndex.Find(GlobalIndex))
				{
					LocalIndex = *Existing;
				}
				else
				{
					const FVoxelMeshVertex& SourceVertex = MeshData.Vertices[GlobalIndex];
					LocalIndex = Positions.Add(ChunkWorldOrigin + SourceVertex.Position * VoxelWorldSize);
					Normals.Add(SourceVertex.Normal);
					UVs.Add(SourceVertex.UV);
					VertexColors.Add(SourceVertex.Color.ToFColor(/*bSRGB=*/true));
					GlobalToLocalVertexIndex.Add(GlobalIndex, LocalIndex);
				}
				Triangles.Add(LocalIndex);
			}

			PMC->CreateMeshSection(SectionIndex, Positions, Triangles, Normals, UVs, VertexColors, Tangents, bEnableCollisionInMeshPreview);

			UMaterialInterface* Material = nullptr;
			if (TObjectPtr<UMaterialInterface>* Override = BlockMaterials.Find(Section.MaterialId))
			{
				Material = *Override;
			}
			else
			{
				Material = DefaultMaterial;
			}
			if (Material)
			{
				PMC->SetMaterial(SectionIndex, Material);
			}

			TotalVertices += Positions.Num();
			TotalTriangles += Triangles.Num() / 3;
		}

		MeshPreviewComponents.Add(PMC);
	}

	UE_LOG(LogVoxelDebug, Log, TEXT("[Mesh preview] %d chunks meshed in %.2f ms total, %d vertices, %d triangles, %d PMC components."),
		MeshPreviewComponents.Num(), TotalMeshingMs, TotalVertices, TotalTriangles, MeshPreviewComponents.Num());
}

void AVoxelDebugVisualizer::GenerateAndVisualizeRendered()
{
	ClearVisualization();

	UVoxelBlockRegistry* LocalRegistry = nullptr;
	TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> GeneratedChunks = GenerateChunkGrid(LocalRegistry);

	int32 TotalVertices = 0;
	int32 TotalTriangles = 0;
	double TotalMeshingMs = 0.0;

	// Deliberately mirrors GenerateAndVisualizeMeshed's structure exactly
	// (same per-chunk granularity, same FVoxelMesher call) so the two modes
	// are a fair, direct comparison - the ONLY difference should be which
	// component consumes the resulting FVoxelMeshData.
	for (const TPair<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>>& Entry : GeneratedChunks)
	{
		const FVoxelChunkCoordinate& Coord = Entry.Key;
		const FVoxelChunk& Chunk = *Entry.Value;

		const double MeshStart = FPlatformTime::Seconds();
		FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, LocalRegistry);
		TotalMeshingMs += (FPlatformTime::Seconds() - MeshStart) * 1000.0;

		if (MeshData.IsEmpty())
		{
			continue;
		}

		TotalVertices += MeshData.Vertices.Num();
		TotalTriangles += MeshData.GetTotalTriangleCount();

		// FVoxelMesher outputs 1-unit-per-voxel local positions - scale by
		// VoxelWorldSize and offset by the chunk's world origin, same
		// convention as the PMC path, so both modes render at the same
		// scale/location for a true side-by-side comparison. UVoxelMeshComponent
		// doesn't know about "chunks" or world scale itself (per ADR-004,
		// it only knows FVoxelMeshData) - baking this into vertex positions
		// here, at the debug-tool call site, is the correct place for it,
		// not inside VoxelRendering.
		const FVector ChunkWorldOrigin(
			Coord.X * ChunkSize * VoxelWorldSize,
			Coord.Y * ChunkSize * VoxelWorldSize,
			Coord.Z * ChunkSize * VoxelWorldSize);

		for (FVoxelMeshVertex& Vertex : MeshData.Vertices)
		{
			Vertex.Position = ChunkWorldOrigin + Vertex.Position * VoxelWorldSize;
		}

		UVoxelMeshComponent* RenderComponent = NewObject<UVoxelMeshComponent>(this,
			*FString::Printf(TEXT("RenderedPreview_%d_%d_%d"), Coord.X, Coord.Y, Coord.Z));
		RenderComponent->SetMobility(EComponentMobility::Movable);
		RenderComponent->SetupAttachment(RootComponent);
		RenderComponent->RegisterComponent();
		RenderComponent->SetCollisionEnabled(bEnableCollisionInMeshPreview ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

		// Assign materials BEFORE SetMeshData, since CreateSceneProxy (triggered
		// by SetMeshData's MarkRenderStateDirty) reads them via GetMaterial().
		for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
		{
			UMaterialInterface* Material = nullptr;
			if (TObjectPtr<UMaterialInterface>* Override = BlockMaterials.Find(MeshData.Sections[SectionIndex].MaterialId))
			{
				Material = *Override;
			}
			else
			{
				Material = DefaultMaterial;
			}
			if (Material)
			{
				RenderComponent->SetMaterial(SectionIndex, Material);
			}
		}

		RenderComponent->SetMeshData(MoveTemp(MeshData));
		RenderedPreviewComponents.Add(RenderComponent);
	}

	UE_LOG(LogVoxelDebug, Log, TEXT("[Real renderer preview] %d chunks meshed in %.2f ms total, %d vertices, %d triangles, %d UVoxelMeshComponent instances. Compare visually against [Mesh preview] output for the same seed/settings."),
		RenderedPreviewComponents.Num(), TotalMeshingMs, TotalVertices, TotalTriangles, RenderedPreviewComponents.Num());
}

void AVoxelDebugVisualizer::RequestChunksViaSubsystem()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("[Subsystem test] Must be run in PIE (Play-In-Editor) - UVoxelWorldSubsystem only exists in game worlds."));
		return;
	}

	UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("[Subsystem test] UVoxelWorldSubsystem not found on this world. Check that the VoxelWorld module is loaded and the subsystem is registered."));
		return;
	}

	// Use the same grid dimensions as the other three modes so the
	// output is directly comparable by eye.
	const int32 HalfRadius = ChunkRadiusXY;
	int32 ChunksRequested = 0;

	for (int32 CX = -HalfRadius; CX < HalfRadius; ++CX)
	{
		for (int32 CY = -HalfRadius; CY < HalfRadius; ++CY)
		{
			for (int32 CZ = 0; CZ < ChunkCountZ; ++CZ)
			{
				const FVoxelChunkCoordinate Coord(CX, CY, CZ);
				Subsystem->RequestChunk(Coord);
				++ChunksRequested;
			}
		}
	}

	UE_LOG(LogVoxelDebug, Log, TEXT("[Subsystem test] Dispatched %d RequestChunk calls (ChunkSize=%d, Seed=%d from subsystem settings). Results will appear asynchronously - watch the viewport."),
		ChunksRequested, Subsystem->GetChunkSize(), Subsystem->GetWorldSeed());
}

void AVoxelDebugVisualizer::ValidateSubsystemResults()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("[Subsystem validate] Must be run in PIE."));
		return;
	}

	UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("[Subsystem validate] UVoxelWorldSubsystem not found."));
		return;
	}

	// Use the subsystem's own seed and chunk size - NOT the debug
	// visualizer's properties - so we're comparing like-for-like.
	const int32 SubChunkSize = Subsystem->GetChunkSize();
	const int32 SubSeed = Subsystem->GetWorldSeed();

	// Build a local pipeline for the synchronous reference generation.
	// No biomes / no registry here - mirrors the subsystem's own pipeline
	// inputs. If the subsystem has biomes configured, the block IDs will
	// differ from this bare generation, but that's fine - the subsystem's
	// FindChunk returns what IT generated, and we regenerate with the same
	// inputs the subsystem used (it resolves biomes internally). For a
	// truly fair comparison we'd need the subsystem to expose its biome
	// list, but for this validation the important signal is determinism:
	// calling FindChunk returns the exact same data the worker thread wrote.
	//
	// NOTE: We can't access the subsystem's private AvailableBiomes/
	// BlockRegistry from here. Instead, we compare the subsystem's output
	// against ITSELF: we verify that for each coordinate, IsChunkReady is
	// true and FindChunk returns non-null with the correct size. Then we
	// check that re-requesting the same chunk is idempotent (no crash,
	// same data). The determinism check (same seed -> same data) is
	// already covered by VoxelGenerationDeterminismTests.

	const int32 HalfRadius = ChunkRadiusXY;
	int32 TotalChunks = 0;
	int32 ReadyChunks = 0;
	int32 PendingChunks = 0;
	int32 PassedChunks = 0;
	int32 FailedChunks = 0;

	UE_LOG(LogVoxelDebug, Log, TEXT("========================================"));
	UE_LOG(LogVoxelDebug, Log, TEXT("[Subsystem validate] Starting validation (Seed=%d, ChunkSize=%d)"), SubSeed, SubChunkSize);
	UE_LOG(LogVoxelDebug, Log, TEXT("========================================"));

	for (int32 CX = -HalfRadius; CX < HalfRadius; ++CX)
	{
		for (int32 CY = -HalfRadius; CY < HalfRadius; ++CY)
		{
			for (int32 CZ = 0; CZ < ChunkCountZ; ++CZ)
			{
				const FVoxelChunkCoordinate Coord(CX, CY, CZ);
				++TotalChunks;

				// --- Check 1: Is it ready? ---
				if (!Subsystem->IsChunkReady(Coord))
				{
					++PendingChunks;
					UE_LOG(LogVoxelDebug, Warning, TEXT("  [PENDING] Chunk (%d,%d,%d) - still generating, try again in a moment."), CX, CY, CZ);
					continue;
				}
				++ReadyChunks;

				// --- Check 2: Can we retrieve the data? ---
				const FVoxelChunk* SubChunk = Subsystem->FindChunk(Coord);
				if (!SubChunk)
				{
					++FailedChunks;
					UE_LOG(LogVoxelDebug, Error, TEXT("  [FAIL] Chunk (%d,%d,%d) - IsChunkReady=true but FindChunk returned nullptr!"), CX, CY, CZ);
					continue;
				}

				// --- Check 3: Correct size? ---
				if (SubChunk->GetSize() != SubChunkSize)
				{
					++FailedChunks;
					UE_LOG(LogVoxelDebug, Error, TEXT("  [FAIL] Chunk (%d,%d,%d) - Size mismatch: expected %d, got %d."), CX, CY, CZ, SubChunkSize, SubChunk->GetSize());
					continue;
				}

				// --- Check 4: Idempotency - re-requesting shouldn't crash or change data ---
				const FVoxelChunkHandle Handle = Subsystem->RequestChunk(Coord);
				if (!Handle.IsValid())
				{
					++FailedChunks;
					UE_LOG(LogVoxelDebug, Error, TEXT("  [FAIL] Chunk (%d,%d,%d) - Re-request returned invalid handle."), CX, CY, CZ);
					continue;
				}

				// Verify data is unchanged after re-request
				const FVoxelChunk* ReChunk = Subsystem->FindChunk(Coord);
				if (ReChunk != SubChunk)
				{
					++FailedChunks;
					UE_LOG(LogVoxelDebug, Error, TEXT("  [FAIL] Chunk (%d,%d,%d) - Re-request returned different chunk pointer (not idempotent)."), CX, CY, CZ);
					continue;
				}

				// --- Check 5: Data sanity - count non-air blocks ---
				int32 SolidCount = 0;
				for (int32 LZ = 0; LZ < SubChunkSize; ++LZ)
				{
					for (int32 LY = 0; LY < SubChunkSize; ++LY)
					{
						for (int32 LX = 0; LX < SubChunkSize; ++LX)
						{
							if (SubChunk->GetBlock(LX, LY, LZ) != VoxelBlockId_Air)
							{
								++SolidCount;
							}
						}
					}
				}

				const int32 TotalVoxels = SubChunkSize * SubChunkSize * SubChunkSize;
				const bool bIsEmpty = SubChunk->IsEmpty();
				const bool bEmptyConsistent = (SolidCount == 0) == bIsEmpty;

				if (!bEmptyConsistent)
				{
					++FailedChunks;
					UE_LOG(LogVoxelDebug, Error, TEXT("  [FAIL] Chunk (%d,%d,%d) - IsEmpty()=%s but found %d/%d solid voxels."),
						CX, CY, CZ, bIsEmpty ? TEXT("true") : TEXT("false"), SolidCount, TotalVoxels);
					continue;
				}

				++PassedChunks;
				UE_LOG(LogVoxelDebug, Log, TEXT("  [PASS] Chunk (%d,%d,%d) - Ready, valid, idempotent, %d/%d solid voxels, IsEmpty consistent."),
					CX, CY, CZ, SolidCount, TotalVoxels);
			}
		}
	}

	UE_LOG(LogVoxelDebug, Log, TEXT("========================================"));
	if (PendingChunks > 0)
	{
		UE_LOG(LogVoxelDebug, Warning, TEXT("[Subsystem validate] %d/%d chunks still pending - click again after they finish."), PendingChunks, TotalChunks);
	}
	if (FailedChunks > 0)
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("[Subsystem validate] FAILED: %d/%d chunks failed validation."), FailedChunks, TotalChunks);
	}
	else if (PendingChunks == 0)
	{
		UE_LOG(LogVoxelDebug, Log, TEXT("[Subsystem validate] ALL PASSED: %d/%d chunks validated successfully."), PassedChunks, TotalChunks);
	}
	else
	{
		UE_LOG(LogVoxelDebug, Log, TEXT("[Subsystem validate] %d passed, %d pending, %d failed out of %d total."), PassedChunks, PendingChunks, FailedChunks, TotalChunks);
	}
	UE_LOG(LogVoxelDebug, Log, TEXT("========================================"));
}
