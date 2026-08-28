// VoxelWorldSubsystem.cpp

#include "VoxelWorldSubsystem.h"
#include "VoxelWorldSettings.h"
#include "AVoxelWorldRenderActor.h"
#include "VoxelChunk.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBiomeDefinition.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelMesher.h"
#include "VoxelMeshData.h"
#include "VoxelMeshComponent.h"
#include "VoxelRuntimeModule.h"
#include "VoxelScheduler.h"
#include "VoxelRuntimeSettings.h"
#include "Materials/MaterialInterface.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelWorld, Log, All);

void UVoxelWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UVoxelRuntimeSettings* RuntimeSettings = GetDefault<UVoxelRuntimeSettings>();
	ChunkSize = RuntimeSettings->ChunkSize;

	const UVoxelWorldSettings* WorldSettings = GetDefault<UVoxelWorldSettings>();
	WorldSeed = WorldSettings->WorldSeed;
	VoxelWorldSize = WorldSettings->VoxelWorldSize;

	ChunkStore = MakeUnique<FVoxelChunkStore>(ChunkSize);

	BlockRegistry = GetWorld()->GetSubsystem<UVoxelBlockRegistry>();
	if (!BlockRegistry)
	{
		UE_LOG(LogVoxelWorld, Warning, TEXT("No UVoxelBlockRegistry available for this world - biome terrain layers will fall back to TerrainPass's flat default layering."));
	}

	// Resolve biome soft pointers ONCE here (Game Thread, Initialize is
	// guaranteed Game-Thread-only) rather than per chunk request - this is
	// the exact TODO item this subsystem exists to satisfy.
	for (const TSoftObjectPtr<UVoxelBiomeDefinition>& SoftBiome : WorldSettings->DefaultBiomes)
	{
		if (UVoxelBiomeDefinition* Biome = SoftBiome.LoadSynchronous())
		{
			ResolvedBiomes.Add(Biome);
			AvailableBiomes.Add(Biome);
		}
	}

	if (BlockRegistry && ResolvedBiomes.Num() > 0)
	{
		BlockRegistry->PrecacheBiomeLayers(ResolvedBiomes);
	}

	for (const TPair<int32, TSoftObjectPtr<UMaterialInterface>>& Pair : WorldSettings->BlockMaterials)
	{
		if (UMaterialInterface* Material = Pair.Value.LoadSynchronous())
		{
			ResolvedBlockMaterials.Add(Pair.Key, Material);
		}
	}
	ResolvedDefaultMaterial = WorldSettings->DefaultMaterial.LoadSynchronous();

	UE_LOG(LogVoxelWorld, Log, TEXT("Initialized: ChunkSize=%d WorldSeed=%d Biomes=%d"), ChunkSize, WorldSeed, ResolvedBiomes.Num());
}

void UVoxelWorldSubsystem::Deinitialize()
{
	// NOTE: any jobs still in flight at this point are NOT cancelled (see
	// class header) - their OnComplete will still fire and call
	// OnChunkMeshReady, which checks WeakThis validity before touching
	// this object, so this is safe, just wasted work. A real cancellation
	// path is VoxelStreaming's job.
	if (RenderHostActor)
	{
		RenderHostActor->Destroy();
		RenderHostActor = nullptr;
	}
	ChunkMeshComponents.Reset();
	ChunkStore.Reset();

	Super::Deinitialize();
}

UVoxelWorldSubsystem::~UVoxelWorldSubsystem() = default;

FVoxelChunkHandle UVoxelWorldSubsystem::RequestChunk(const FVoxelChunkCoordinate& Coordinate)
{
	check(IsInGameThread());

	const FVoxelChunkHandle ExistingHandle = ChunkStore->CreateOrGetChunk(Coordinate);

	if (RequestedCoordinates.Contains(Coordinate))
	{
		return ExistingHandle; // already dispatched or completed - idempotent, no second job
	}
	RequestedCoordinates.Add(Coordinate);

	FVoxelChunk* Chunk = ChunkStore->FindChunkByHandle(ExistingHandle);
	check(Chunk); // just created above, must be valid

	const int32 CapturedSeed = WorldSeed;
	const int32 CapturedChunkSize = ChunkSize;
	const UVoxelBlockRegistry* CapturedRegistry = BlockRegistry;
	TArray<const UVoxelBiomeDefinition*> CapturedBiomes = AvailableBiomes;
	TWeakObjectPtr<UVoxelWorldSubsystem> WeakThis(this);

	// Result stashed here so the worker-thread Work lambda and the
	// (possibly different-thread) OnComplete lambda can share it without
	// FVoxelScheduler's API needing to support returning a value directly.
	TSharedRef<FVoxelMeshData, ESPMode::ThreadSafe> MeshDataResult = MakeShared<FVoxelMeshData, ESPMode::ThreadSafe>();

	const FVoxelJobHandle JobHandle = FVoxelRuntimeModule::Get().GetScheduler().Submit(
		[Chunk, CapturedSeed, Coordinate, CapturedChunkSize, CapturedRegistry, CapturedBiomes, MeshDataResult]()
		{
			// Worker thread. Both calls are documented worker-thread-safe,
			// deterministic, no UObject writes (VoxelGeneration/VoxelMeshing
			// contracts) - see Docs/ARCHITECTURE.md #5, #5.1.
			FVoxelGenerationPipeline Pipeline; // stateless/const - fine to construct per-job; sharing one instance across jobs is a possible future optimization, not a correctness concern
			Pipeline.GenerateChunk(CapturedSeed, Coordinate, CapturedChunkSize, CapturedRegistry, CapturedBiomes, *Chunk);
			*MeshDataResult = FVoxelMesher::GenerateMesh(*Chunk, CapturedRegistry);
		},
		EVoxelWorkPriority::Normal,
		[WeakThis, Coordinate, MeshDataResult]()
		{
			// OnComplete runs on the completing thread, NOT guaranteed Game
			// Thread (FVoxelScheduler contract) - marshal explicitly, same
			// pattern documented in VoxelMeshingService.
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Coordinate, MeshDataResult]()
			{
				if (UVoxelWorldSubsystem* StrongThis = WeakThis.Get())
				{
					StrongThis->OnChunkMeshReady(Coordinate, MoveTemp(*MeshDataResult));
				}
				// If StrongThis is invalid, the subsystem was destroyed
				// while this job was in flight - discard the result, see
				// Deinitialize's comment on why this is safe.
			});
		});

	InFlightJobHandles.Add(Coordinate, JobHandle);

	return ExistingHandle;
}

void UVoxelWorldSubsystem::OnChunkMeshReady(FVoxelChunkCoordinate Coordinate, FVoxelMeshData&& MeshData)
{
	check(IsInGameThread());

	// Remove from in-flight tracking (job is done, regardless of outcome).
	InFlightJobHandles.Remove(Coordinate);

	// Guard against zombie chunks: if UnloadChunk was called while this job
	// was running, RequestedCoordinates no longer contains this coordinate.
	// RequestCancel only prevents Queued jobs from starting — a Running
	// job's Work() completes normally and OnComplete still fires. This
	// guard is the actual fix for that timing, not optional defense-in-depth.
	if (!RequestedCoordinates.Contains(Coordinate))
	{
		return;
	}

	ReadyCoordinates.Add(Coordinate);

	if (MeshData.IsEmpty())
	{
		return; // all-air chunk - valid ready state, nothing to render
	}

	// VoxelRendering has zero opinion on world scale/position (per ADR-004,
	// it only knows FVoxelMeshData) - baking this in at the call site is
	// correct, same convention VoxelDebug's real-renderer preview uses.
	const FVector ChunkWorldOrigin(
		Coordinate.X * ChunkSize * VoxelWorldSize,
		Coordinate.Y * ChunkSize * VoxelWorldSize,
		Coordinate.Z * ChunkSize * VoxelWorldSize);
	for (FVoxelMeshVertex& Vertex : MeshData.Vertices)
	{
		Vertex.Position = ChunkWorldOrigin + Vertex.Position * VoxelWorldSize;
	}

	UVoxelMeshComponent* Component = GetOrCreateMeshComponent(Coordinate);

	for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
	{
		if (UMaterialInterface* Material = ResolveMaterialForId(MeshData.Sections[SectionIndex].MaterialId))
		{
			Component->SetMaterial(SectionIndex, Material);
		}
	}

	Component->SetMeshData(MoveTemp(MeshData));
}

UVoxelMeshComponent* UVoxelWorldSubsystem::GetOrCreateMeshComponent(const FVoxelChunkCoordinate& Coordinate)
{
	if (TWeakObjectPtr<UVoxelMeshComponent>* Existing = ChunkMeshComponents.Find(Coordinate))
	{
		return Existing->Get();
	}

	if (!RenderHostActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		RenderHostActor = GetWorld()->SpawnActor<AVoxelWorldRenderActor>(SpawnParams);
	}

	UVoxelMeshComponent* Component = NewObject<UVoxelMeshComponent>(RenderHostActor,
		*FString::Printf(TEXT("ChunkMesh_%d_%d_%d"), Coordinate.X, Coordinate.Y, Coordinate.Z));
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetupAttachment(RenderHostActor->GetRootComponent());
	Component->RegisterComponent();

	ChunkMeshComponents.Add(Coordinate, Component);
	return Component;
}

UMaterialInterface* UVoxelWorldSubsystem::ResolveMaterialForId(int32 MaterialId) const
{
	if (const TObjectPtr<UMaterialInterface>* Override = ResolvedBlockMaterials.Find(MaterialId))
	{
		return *Override;
	}
	return ResolvedDefaultMaterial;
}

void UVoxelWorldSubsystem::UnloadChunk(const FVoxelChunkCoordinate& Coordinate)
{
	check(IsInGameThread());

	if (TWeakObjectPtr<UVoxelMeshComponent>* Component = ChunkMeshComponents.Find(Coordinate))
	{
		if (Component->IsValid())
		{
			Component->Get()->DestroyComponent();
		}
		ChunkMeshComponents.Remove(Coordinate);
	}

	ChunkStore->RemoveChunk(Coordinate);
	RequestedCoordinates.Remove(Coordinate);
	ReadyCoordinates.Remove(Coordinate);
	// Cancel any in-flight generation/meshing job for this coordinate.
	// RequestCancel prevents Queued jobs from starting Work(). For Running
	// jobs, Work() finishes but OnChunkMeshReady's guard (checking
	// RequestedCoordinates) discards the result — see that method's comment.
	if (const FVoxelJobHandle* JobHandle = InFlightJobHandles.Find(Coordinate))
	{
		FVoxelRuntimeModule::Get().GetScheduler().RequestCancel(*JobHandle);
		InFlightJobHandles.Remove(Coordinate);
	}
}

const FVoxelChunk* UVoxelWorldSubsystem::FindChunk(const FVoxelChunkCoordinate& Coordinate) const
{
	return ChunkStore ? ChunkStore->FindChunkByCoordinate(Coordinate) : nullptr;
}

bool UVoxelWorldSubsystem::IsChunkReady(const FVoxelChunkCoordinate& Coordinate) const
{
	return ReadyCoordinates.Contains(Coordinate);
}

void UVoxelWorldSubsystem::SetChunkVisible(const FVoxelChunkCoordinate& Coordinate, bool bVisible)
{
	check(IsInGameThread());

	if (const TWeakObjectPtr<UVoxelMeshComponent>* WeakComp = ChunkMeshComponents.Find(Coordinate))
	{
		if (WeakComp->IsValid())
		{
			WeakComp->Get()->SetVisibility(bVisible, /*bPropagateToChildren=*/ true);
		}
	}
}
