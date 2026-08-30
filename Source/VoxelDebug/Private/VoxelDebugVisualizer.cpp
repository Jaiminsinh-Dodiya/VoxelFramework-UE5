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
#include "VoxelStreamingManager.h"
#include "VoxelRuntimeSettings.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "RHI.h"

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
	if (const TSoftObjectPtr<UMaterialInterface>* Override = BlockMaterials.Find(BlockId))
	{
		Material = Override->LoadSynchronous();
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
					LocalIndex = Positions.Add(ChunkWorldOrigin + FVector(SourceVertex.Position) * VoxelWorldSize);
					Normals.Add(FVector(SourceVertex.Normal));
					UVs.Add(FVector2D(SourceVertex.UV));
					VertexColors.Add(SourceVertex.Color);
					GlobalToLocalVertexIndex.Add(GlobalIndex, LocalIndex);
				}
				Triangles.Add(LocalIndex);
			}

			PMC->CreateMeshSection(SectionIndex, Positions, Triangles, Normals, UVs, VertexColors, Tangents, bEnableCollisionInMeshPreview);

			UMaterialInterface* Material = nullptr;
			if (const TSoftObjectPtr<UMaterialInterface>* Override = BlockMaterials.Find(Section.MaterialId))
			{
				Material = Override->LoadSynchronous();
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

	for (const TPair<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>>& Entry : GeneratedChunks)
	{
		const FVoxelChunkCoordinate& Coord = Entry.Key;
		const FVoxelChunk& Chunk = *Entry.Value;

		const double MeshStart = FPlatformTime::Seconds();
		FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, LocalRegistry, nullptr, &Coord, VoxelWorldSize);
		TotalMeshingMs += (FPlatformTime::Seconds() - MeshStart) * 1000.0;

		if (MeshData.IsEmpty())
		{
			continue;
		}

		TotalVertices += MeshData.Vertices.Num();
		TotalTriangles += MeshData.GetTotalTriangleCount();

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
			if (const TSoftObjectPtr<UMaterialInterface>* Override = BlockMaterials.Find(MeshData.Sections[SectionIndex].MaterialId))
			{
				Material = Override->LoadSynchronous();
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

void AVoxelDebugVisualizer::StartPerformanceDiagnostics()
{
	if (bDiagnosticsRunning)
	{
		UE_LOG(LogVoxelDebug, Warning, TEXT("[Diagnostics] Diagnostics already running."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogVoxelDebug, Error, TEXT("[Diagnostics] Must be run in PIE (Play-In-Editor) - world subsystems and live frame metrics are only active in game worlds."));
		return;
	}

	ResetDiagnosticStats();
	bDiagnosticsRunning = true;

	// Run initial tick immediately
	DiagnosticsTick(0.0f);

	// Register 10 Hz ticker (0.1s interval)
	DiagnosticsTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &AVoxelDebugVisualizer::DiagnosticsTick),
		0.1f);

	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Started live performance diagnostics (10 Hz). Watch viewport HUD."));
}

void AVoxelDebugVisualizer::StopPerformanceDiagnostics()
{
	if (!bDiagnosticsRunning && !DiagnosticsTickerHandle.IsValid())
	{
		return; // Idempotent
	}

	bDiagnosticsRunning = false;

	if (DiagnosticsTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DiagnosticsTickerHandle);
		DiagnosticsTickerHandle.Reset();
	}

	if (GEngine)
	{
		for (int32 i = 0; i < DiagnosticsLineCount; ++i)
		{
			GEngine->RemoveOnScreenDebugMessage(GetDiagnosticsKey(i));
		}
	}

	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Performance diagnostics stopped and overlay removed."));
}

void AVoxelDebugVisualizer::ApplyModeA_Baseline()
{
	ClearVisualization();
	ActiveDiagnosticMode = EVoxelDiagnosticMode::ModeA_Baseline;
	UWorld* World = GetWorld();
	if (World)
	{
		if (UVoxelStreamingManager* StreamingManager = World->GetSubsystem<UVoxelStreamingManager>())
		{
			StreamingManager->SetStreamingFrozen(true);
			StreamingManager->ClearAllManaged();
		}
		if (UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>())
		{
			Subsystem->ClearAllChunks();
		}
	}
	ResetDiagnosticStats();
	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Mode A Applied: Baseline (Voxel Framework OFF). Measuring pure engine/scene baseline cost."));
}

void AVoxelDebugVisualizer::ApplyModeB_VoxelRenderingOn()
{
	ClearVisualization();
	ActiveDiagnosticMode = EVoxelDiagnosticMode::ModeB_VoxelRenderingOn;
	UWorld* World = GetWorld();
	if (World)
	{
		if (UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>())
		{
			Subsystem->SetCpuOnlyMode(false);
			Subsystem->SetCastShadows(bVoxelCastShadows);
		}
		if (UVoxelStreamingManager* StreamingManager = World->GetSubsystem<UVoxelStreamingManager>())
		{
			StreamingManager->SetStreamingFrozen(false);
			StreamingManager->ForceReevaluateQueue();
		}
	}
	ResetDiagnosticStats();
	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Mode B Applied: Normal Voxel Rendering ON."));
}

void AVoxelDebugVisualizer::ApplyModeC_CpuOnly()
{
	ClearVisualization();
	ActiveDiagnosticMode = EVoxelDiagnosticMode::ModeC_CpuOnly;
	UWorld* World = GetWorld();
	if (World)
	{
		if (UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>())
		{
			Subsystem->SetCpuOnlyMode(true);
			Subsystem->ClearAllChunks();
		}
		if (UVoxelStreamingManager* StreamingManager = World->GetSubsystem<UVoxelStreamingManager>())
		{
			StreamingManager->ClearAllManaged();
			StreamingManager->SetStreamingFrozen(false);
			StreamingManager->ForceReevaluateQueue();
		}
	}
	ResetDiagnosticStats();
	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Mode C Applied: CPU Generation & Meshing isolation (Render Components & GPU Work Bypassed)."));
}

void AVoxelDebugVisualizer::ApplyModeD_StaticWorld()
{
	ActiveDiagnosticMode = EVoxelDiagnosticMode::ModeD_StaticWorld;
	UWorld* World = GetWorld();
	if (World)
	{
		if (UVoxelStreamingManager* StreamingManager = World->GetSubsystem<UVoxelStreamingManager>())
		{
			StreamingManager->SetStreamingFrozen(true);
		}
	}
	ResetDiagnosticStats();
	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Mode D Applied: Static World (Streaming Updates Frozen). Measuring steady-state rendering cost."));
}

void AVoxelDebugVisualizer::ApplyModeE_StreamingStress()
{
	ActiveDiagnosticMode = EVoxelDiagnosticMode::ModeE_StreamingStress;
	UWorld* World = GetWorld();
	if (World)
	{
		if (UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>())
		{
			Subsystem->SetCpuOnlyMode(false);
		}
		if (UVoxelStreamingManager* StreamingManager = World->GetSubsystem<UVoxelStreamingManager>())
		{
			StreamingManager->SetStreamingFrozen(false);
			StreamingManager->ForceReevaluateQueue();
		}
	}
	ResetDiagnosticStats();
	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Mode E Applied: Streaming Stress."));
}

void AVoxelDebugVisualizer::ToggleVoxelShadows()
{
	bVoxelCastShadows = !bVoxelCastShadows;
	UWorld* World = GetWorld();
	if (World)
	{
		if (UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>())
		{
			Subsystem->SetCastShadows(bVoxelCastShadows);
		}
	}
	ResetDiagnosticStats();
	UE_LOG(LogVoxelDebug, Log, TEXT("[Diagnostics] Voxel Dynamic Shadow Casting: %s"), bVoxelCastShadows ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void AVoxelDebugVisualizer::ResetDiagnosticStats()
{
	FrameTimeHistory.Reset();
	FramesOver16Ms = 0;
	FramesOver33Ms = 0;
	FramesOver50Ms = 0;
	MinFrameTimeMs = FLT_MAX;
	MaxFrameTimeMs = 0.0f;
	TotalFrameTimeAccumMs = 0.0f;
	TotalFramesSampled = 0;

	if (UWorld* World = GetWorld())
	{
		if (UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>())
		{
			Subsystem->ResetLatencyStats();
		}
	}
}

float AVoxelDebugVisualizer::CalculatePercentile(float Percentile) const
{
	if (FrameTimeHistory.Num() == 0)
	{
		return 0.0f;
	}
	TArray<float> Sorted = FrameTimeHistory;
	Sorted.Sort();
	const int32 Index = FMath::Clamp(FMath::RoundToInt(Percentile * (Sorted.Num() - 1)), 0, Sorted.Num() - 1);
	return Sorted[Index];
}

void AVoxelDebugVisualizer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPerformanceDiagnostics();
	Super::EndPlay(EndPlayReason);
}

void AVoxelDebugVisualizer::BeginDestroy()
{
	StopPerformanceDiagnostics();
	Super::BeginDestroy();
}

bool AVoxelDebugVisualizer::DiagnosticsTick(float DeltaTime)
{
	if (!bDiagnosticsRunning)
	{
		return false; // Unregister ticker
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !GEngine)
	{
		return true; // Keep ticker alive
	}

	// 1. Engine & Frame Timings
	const float DeltaSeconds = FApp::GetDeltaTime();
	const float InstantFps = (DeltaSeconds > 0.0f) ? (1.0f / DeltaSeconds) : 0.0f;
	const float FrameMs = DeltaSeconds * 1000.0f;
	const float IdleMs = FApp::GetIdleTime() * 1000.0f;
	const float WorkMs = FMath::Max(0.0f, FrameMs - IdleMs);

	// Accumulate stats for frame pacing
	TotalFramesSampled++;
	TotalFrameTimeAccumMs += FrameMs;
	MinFrameTimeMs = FMath::Min(MinFrameTimeMs, FrameMs);
	MaxFrameTimeMs = FMath::Max(MaxFrameTimeMs, FrameMs);
	if (FrameMs > 16.67f) FramesOver16Ms++;
	if (FrameMs > 33.33f) FramesOver33Ms++;
	if (FrameMs > 50.00f) FramesOver50Ms++;

	FrameTimeHistory.Add(FrameMs);
	if (FrameTimeHistory.Num() > MaxFrameHistorySamples)
	{
		FrameTimeHistory.RemoveAt(0, EAllowShrinking::No);
	}

	const float AvgFrameMs = (TotalFramesSampled > 0) ? (TotalFrameTimeAccumMs / TotalFramesSampled) : FrameMs;
	const float P95Ms = CalculatePercentile(0.95f);
	const float P99Ms = CalculatePercentile(0.99f);
	const float PctOver16 = (TotalFramesSampled > 0) ? (100.0f * FramesOver16Ms / TotalFramesSampled) : 0.0f;
	const float PctOver33 = (TotalFramesSampled > 0) ? (100.0f * FramesOver33Ms / TotalFramesSampled) : 0.0f;

	const float GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const uint32 GpuCycles = RHIGetGPUFrameCycles(0);
	const float GpuMs = (GpuCycles > 0) ? FPlatformTime::ToMilliseconds(GpuCycles) : 0.0f;

	// 2. Memory Stats
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	const double PhysicalRamMb = static_cast<double>(MemStats.UsedPhysical) / (1024.0 * 1024.0);
	const double PeakRamMb = static_cast<double>(MemStats.PeakUsedPhysical) / (1024.0 * 1024.0);

	// 3. Subsystem Metrics
	UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>();
	UVoxelStreamingManager* StreamingManager = World->GetSubsystem<UVoxelStreamingManager>();

	const int32 ChunkSizeLocal = Subsystem ? Subsystem->GetChunkSize() : ChunkSize;
	const int32 ManagedCount = StreamingManager ? StreamingManager->GetManagedChunkCount() : 0;
	const int32 VisibleCount = StreamingManager ? StreamingManager->GetVisibleChunkCount() : 0;
	const int32 ReadyCount = Subsystem ? Subsystem->GetReadyChunkCount() : 0;
	const int32 PendingRequests = StreamingManager ? StreamingManager->GetPendingRequestCount() : 0;
	const int32 PendingUnloads = StreamingManager ? StreamingManager->GetPendingUnloadCount() : 0;
	const float LastStreamingTickMs = StreamingManager ? StreamingManager->GetLastTickBudgetUsedMs() : 0.0f;

	const int32 FinalizeQueueDepth = Subsystem ? Subsystem->GetFinalizationQueueDepth() : 0;
	const float LastFinalizeMs = Subsystem ? Subsystem->GetLastFinalizeBudgetUsedMs() : 0.0f;
	const int32 LastFinalizeCount = Subsystem ? Subsystem->GetLastFinalizeCount() : 0;

	const UVoxelRuntimeSettings* RuntimeSettings = GetDefault<UVoxelRuntimeSettings>();
	const float StreamingBudgetLimitMs = RuntimeSettings ? RuntimeSettings->StreamingBudgetMs : 1.5f;
	const float RenderSubmissionBudgetLimitMs = RuntimeSettings ? RuntimeSettings->RenderSubmissionBudgetMs : 1.0f;

	const double RawVoxelMemoryMb = static_cast<double>(ManagedCount) * (ChunkSizeLocal * ChunkSizeLocal * ChunkSizeLocal * 2) / (1024.0 * 1024.0);

	// 4. Color Evaluation
	const FColor ColorGreen(50, 230, 50);
	const FColor ColorYellow(240, 210, 40);
	const FColor ColorRed(240, 50, 50);
	const FColor ColorCyan(60, 200, 255);
	const FColor ColorWhite(230, 230, 230);
	const FColor ColorOrange(255, 140, 30);

	auto GetFpsColor = [&](float InFps) -> FColor
	{
		if (InFps >= 55.0f) return ColorGreen;
		if (InFps >= 30.0f) return ColorYellow;
		return ColorRed;
	};

	auto GetFrameTimeColor = [&](float InMs) -> FColor
	{
		if (InMs <= 16.67f) return ColorGreen;  // 60+ FPS target
		if (InMs <= 33.33f) return ColorYellow; // 30-60 FPS target
		return ColorRed;                        // <30 FPS
	};

	auto GetBudgetColor = [&](float InMs, float BudgetMs) -> FColor
	{
		if (InMs <= BudgetMs) return ColorGreen;
		if (InMs <= BudgetMs * 1.33f) return ColorYellow;
		return ColorRed;
	};

	const int32 ActiveCompCount = Subsystem ? Subsystem->GetActiveComponentCount() : 0;
	const int32 PooledCompCount = Subsystem ? Subsystem->GetPooledComponentCount() : 0;
	const int32 CreatedCompCount = Subsystem ? Subsystem->GetCreatedComponentCount() : 0;
	const int32 ReusedCompCount = Subsystem ? Subsystem->GetReusedComponentCount() : 0;
	const int32 DestroyedCompCount = Subsystem ? Subsystem->GetDestroyedComponentCount() : 0;
	const int32 PeakPoolCount = Subsystem ? Subsystem->GetPeakPoolSize() : 0;

	const float AvgQueueLatencyMs = Subsystem ? Subsystem->GetAverageQueueLatencyMs() : 0.0f;
	const float P50QueueLatencyMs = Subsystem ? Subsystem->CalculateQueueLatencyPercentile(0.50f) : 0.0f;
	const float P95QueueLatencyMs = Subsystem ? Subsystem->CalculateQueueLatencyPercentile(0.95f) : 0.0f;
	const float P99QueueLatencyMs = Subsystem ? Subsystem->CalculateQueueLatencyPercentile(0.99f) : 0.0f;
	const float MaxQueueLatencyMs = Subsystem ? Subsystem->GetMaxQueueLatencyMs() : 0.0f;
	const float OldestItemAgeMs = Subsystem ? Subsystem->GetOldestQueueItemAgeMs() : 0.0f;

	// 5. Draw On-Screen Debug Lines (0.3s display duration for smooth 10 Hz updates)
	const float DisplayDuration = 0.3f;

	FString ModeStr = TEXT("Mode B (Voxel ON)");
	if (ActiveDiagnosticMode == EVoxelDiagnosticMode::ModeA_Baseline) ModeStr = TEXT("Mode A (Baseline / Voxel OFF)");
	else if (ActiveDiagnosticMode == EVoxelDiagnosticMode::ModeC_CpuOnly) ModeStr = TEXT("Mode C (CPU Generation/Meshing Isolation)");
	else if (ActiveDiagnosticMode == EVoxelDiagnosticMode::ModeD_StaticWorld) ModeStr = TEXT("Mode D (Static World / Streaming Frozen)");
	else if (ActiveDiagnosticMode == EVoxelDiagnosticMode::ModeE_StreamingStress) ModeStr = TEXT("Mode E (Streaming Stress)");

	// Line 0: Header with Mode & Shadow Status
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(0), DisplayDuration, ColorCyan,
		FString::Printf(TEXT("[VOXEL DIAGNOSTICS - 10 Hz] %s | Shadows: %s | Samples: %d"),
			*ModeStr, bVoxelCastShadows ? TEXT("ON") : TEXT("OFF"), TotalFramesSampled));

	// Line 1: FPS / Frame Interval + Min / Max / Avg
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(1), DisplayDuration, GetFpsColor(InstantFps),
		FString::Printf(TEXT("  FPS: %.1f fps (%.2f ms) | Avg: %.2f ms | Min: %.2f ms | Max Spike: %.2f ms"),
			InstantFps, FrameMs, AvgFrameMs, (MinFrameTimeMs == FLT_MAX ? 0.0f : MinFrameTimeMs), MaxFrameTimeMs));

	// Line 2: Frame Pacing Percentiles
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(2), DisplayDuration, GetFrameTimeColor(P95Ms),
		FString::Printf(TEXT("  P95: %.2f ms | P99: %.2f ms | >16.7ms (60fps): %.1f%% | >33.3ms (30fps): %.1f%% | Spikes >50ms: %d"),
			P95Ms, P99Ms, PctOver16, PctOver33, FramesOver50Ms));

	// Line 3: Thread Timing Breakdown (Game Thread, Render Thread, GPU)
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(3), DisplayDuration, ColorWhite,
		FString::Printf(TEXT("  Thread Breakdown: GameThread: %.2f ms | RenderThread: %.2f ms | GPU: %.2f ms | Frame Work: %.2f ms"),
			GameThreadMs, RenderThreadMs, GpuMs, WorkMs));

	// Line 4: Streaming Work vs Budget
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(4), DisplayDuration, GetBudgetColor(LastStreamingTickMs, StreamingBudgetLimitMs),
		FString::Printf(TEXT("  Streaming Tick: %.2f ms / Budget: %.2f ms | Queue: %d Req, %d Unload"),
			LastStreamingTickMs, StreamingBudgetLimitMs, PendingRequests, PendingUnloads));

	// Line 5: GT Mesh Finalize vs Budget
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(5), DisplayDuration, GetBudgetColor(LastFinalizeMs, RenderSubmissionBudgetLimitMs),
		FString::Printf(TEXT("  GT Mesh Finalize: %.2f ms / Budget: %.2f ms | Finalized: %d chunks | Pending Finalize Queue: %d"),
			LastFinalizeMs, RenderSubmissionBudgetLimitMs, LastFinalizeCount, FinalizeQueueDepth));

	// Line 6: Finalization Queue Telemetry (Oldest Pending Age vs Dequeued Latency)
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(6), DisplayDuration, (AvgQueueLatencyMs <= 33.3f) ? ColorGreen : ColorYellow,
		FString::Printf(TEXT("  Finalize Queue: Depth: %d | Oldest Pending: %.1f ms | Dequeued Latency (Avg: %.1f ms, P50: %.1f ms, P95: %.1f ms, Max: %.1f ms)"),
			FinalizeQueueDepth, OldestItemAgeMs, AvgQueueLatencyMs, P50QueueLatencyMs, P95QueueLatencyMs, MaxQueueLatencyMs));

	// Line 7: Component Pool Telemetry
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(7), DisplayDuration, ColorWhite,
		FString::Printf(TEXT("  Component Pool: Active: %d | Pooled: %d (Peak: %d) | Created: %d | Reused: %d | Destroyed: %d"),
			ActiveCompCount, PooledCompCount, PeakPoolCount, CreatedCompCount, ReusedCompCount, DestroyedCompCount));

	// Line 8: Chunk Residency
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(8), DisplayDuration, ColorWhite,
		FString::Printf(TEXT("  Chunks: %d Managed | %d Ready | %d Visible (Non-Nanite Casters)"),
			ManagedCount, ReadyCount, VisibleCount));

	// Line 9: Memory Footprint
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(9), DisplayDuration, ColorWhite,
		FString::Printf(TEXT("  Raw Voxel Memory: %.2f MB | Physical RAM: %.1f MB (Peak: %.1f MB)"),
			RawVoxelMemoryMb, PhysicalRamMb, PeakRamMb));

	// Line 10: VSM Warning / Shadow Context Note
	const bool bVsmRisk = bVoxelCastShadows && (VisibleCount > 60);
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(10), DisplayDuration, bVsmRisk ? ColorOrange : ColorWhite,
		FString::Printf(TEXT("  VSM Non-Nanite Casters: %d | Shadow Setting: %s %s"),
			VisibleCount, bVoxelCastShadows ? TEXT("Enabled") : TEXT("Disabled"),
			bVsmRisk ? TEXT("[WARN: High Non-Nanite shadow count can trigger VSM marking overflow]") : TEXT("")));

	// Line 11: Target Compliance Summary
	const bool bPassing60 = (AvgFrameMs <= 16.67f) && (P95Ms <= 18.0f) && (FramesOver50Ms == 0);
	const bool bPassing30 = (AvgFrameMs <= 33.33f) && (P95Ms <= 35.0f);
	GEngine->AddOnScreenDebugMessage(
		GetDiagnosticsKey(11), DisplayDuration, bPassing60 ? ColorGreen : (bPassing30 ? ColorYellow : ColorRed),
		FString::Printf(TEXT("  Target Compliance: 60 FPS Target: %s | 30 FPS Target: %s"),
			bPassing60 ? TEXT("PASS") : TEXT("FAIL/WARN"),
			bPassing30 ? TEXT("PASS") : TEXT("FAIL")));

	return true; // Keep ticking
}

