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
	RenderSubmissionBudgetMs = RuntimeSettings->RenderSubmissionBudgetMs;

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
	// guaranteed Game-Thread-only) rather than per chunk request.
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

	GenerationPipeline = MakeShared<FVoxelGenerationPipeline>();

	UE_LOG(LogVoxelWorld, Log, TEXT("Initialized: ChunkSize=%d WorldSeed=%d Biomes=%d RenderBudget=%.1fms"),
		ChunkSize, WorldSeed, ResolvedBiomes.Num(), RenderSubmissionBudgetMs);
}

void UVoxelWorldSubsystem::Deinitialize()
{
	ClearAllChunks();

	for (TObjectPtr<UVoxelMeshComponent>& PooledComp : ComponentPool)
	{
		if (IsValid(PooledComp))
		{
			PooledComp->DestroyComponent();
			DestroyedComponentCount++;
		}
	}
	ComponentPool.Empty();

	if (RenderHostActor)
	{
		RenderHostActor->Destroy();
		RenderHostActor = nullptr;
	}

	GenerationPipeline.Reset();
	ChunkStore.Reset();
	Super::Deinitialize();
}

UVoxelWorldSubsystem::~UVoxelWorldSubsystem()
{
}

void UVoxelWorldSubsystem::Tick(float DeltaTime)
{
	ProcessCompletedMeshQueue(DeltaTime);
}

TStatId UVoxelWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVoxelWorldSubsystem, STATGROUP_Tickables);
}

void UVoxelWorldSubsystem::ProcessCompletedMeshQueue(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GTFinalizeMesh);

	if (FinalizationQueueDepth.Load() <= 0)
	{
		LastFinalizeBudgetUsedMs = 0.0f;
		LastFinalizeCount = 0;
		OldestQueueItemAgeMs = 0.0f;
		return;
	}

	const double StartSec = FPlatformTime::Seconds();
	const double BudgetSec = RenderSubmissionBudgetMs * 0.001;
	const int32 MaxMeshesPerTick = 4; // Bounded count to guarantee zero single-frame spikes
	int32 ProcessedCount = 0;

	FVoxelCompletedMeshItem Item;
	while (CompletedMeshQueue.Dequeue(Item))
	{
		FinalizationQueueDepth--;

		const double Now = FPlatformTime::Seconds();
		const float LatencyMs = static_cast<float>((Now - Item.QueueEntryTime) * 1000.0);

		TotalFinalizedItemsSampled++;
		TotalQueueLatencyAccumMs += LatencyMs;
		AverageQueueLatencyMs = static_cast<float>(TotalQueueLatencyAccumMs / TotalFinalizedItemsSampled);
		MaxQueueLatencyMs = FMath::Max(MaxQueueLatencyMs, LatencyMs);
		OldestQueueItemAgeMs = LatencyMs;

		QueueLatencyHistory.Add(LatencyMs);
		if (QueueLatencyHistory.Num() > MaxQueueLatencySamples)
		{
			QueueLatencyHistory.RemoveAt(0, EAllowShrinking::No);
		}

		FinalizeChunkMesh(MoveTemp(Item));
		ProcessedCount++;

		if ((FPlatformTime::Seconds() - StartSec) >= BudgetSec || ProcessedCount >= MaxMeshesPerTick)
		{
			break;
		}
	}

	LastFinalizeBudgetUsedMs = static_cast<float>((FPlatformTime::Seconds() - StartSec) * 1000.0);
	LastFinalizeCount = ProcessedCount;
}

float UVoxelWorldSubsystem::CalculateQueueLatencyPercentile(float Percentile) const
{
	if (QueueLatencyHistory.Num() == 0)
	{
		return 0.0f;
	}
	TArray<float> Sorted = QueueLatencyHistory;
	Sorted.Sort();
	const int32 Index = FMath::Clamp(FMath::RoundToInt(Percentile * (Sorted.Num() - 1)), 0, Sorted.Num() - 1);
	return Sorted[Index];
}

void UVoxelWorldSubsystem::FinalizeChunkMesh(FVoxelCompletedMeshItem&& Item)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MeshUpload);
	check(IsInGameThread());

	const FVoxelChunkCoordinate Coordinate = Item.Coordinate;
	InFlightJobHandles.Remove(Coordinate);
	InFlightCancelFlags.Remove(Coordinate);
	PendingRemeshCoordinates.Remove(Coordinate);

	// Guard against unloads while work was in flight or queued
	if (!RequestedCoordinates.Contains(Coordinate))
	{
		ChunkStates.Remove(Coordinate);
		if (ChunkStore && Item.SlotIndex != INDEX_NONE)
		{
			ChunkStore->ReleaseWorkerLease(Item.SlotIndex);
		}
		return;
	}

	ReadyCoordinates.Add(Coordinate);
	ChunkStates.Add(Coordinate, EVoxelChunkState::Ready);

	// In CPU-only Mode C, we bypass render component creation and GPU work
	if (!bCpuOnlyMode && !Item.MeshData.IsEmpty())
	{
		UVoxelMeshComponent* Component = GetOrCreateMeshComponent(Coordinate);

		for (int32 SectionIndex = 0; SectionIndex < Item.MeshData.Sections.Num(); ++SectionIndex)
		{
			if (UMaterialInterface* Material = ResolveMaterialForId(Item.MeshData.Sections[SectionIndex].MaterialId))
			{
				Component->SetMaterial(SectionIndex, Material);
			}
		}

		Component->SetMeshData(MoveTemp(Item.MeshData));
	}

	if (ChunkStore && Item.SlotIndex != INDEX_NONE)
	{
		ChunkStore->ReleaseWorkerLease(Item.SlotIndex);
	}

	// Neighbor Arrival Remesh Trigger: ONLY trigger on initial chunk generation (!Item.bIsRemesh),
	// NEVER trigger recursively when a remesh job completes.
	if (!Item.bIsRemesh)
	{
		const FVoxelChunkCoordinate CardinalNeighbors[6] = {
			FVoxelChunkCoordinate(Coordinate.X - 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X + 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y - 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y + 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z - 1),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z + 1)
		};

		for (const FVoxelChunkCoordinate& NeighborCoord : CardinalNeighbors)
		{
			if (IsChunkReady(NeighborCoord) && !InFlightJobHandles.Contains(NeighborCoord) && !PendingRemeshCoordinates.Contains(NeighborCoord))
			{
				RequestRemeshChunk(NeighborCoord, EVoxelWorkPriority::Low);
			}
		}
	}
}

FVoxelChunkHandle UVoxelWorldSubsystem::RequestChunk(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority)
{
	check(IsInGameThread());

	const FVoxelChunkHandle ExistingHandle = ChunkStore->CreateOrGetChunk(Coordinate);

	if (RequestedCoordinates.Contains(Coordinate))
	{
		return ExistingHandle; // Idempotent - already requested or ready
	}

	RequestedCoordinates.Add(Coordinate);
	ChunkStates.Add(Coordinate, EVoxelChunkState::Queued);

	FVoxelChunk* Chunk = ChunkStore->FindChunkByHandle(ExistingHandle);
	check(Chunk);

	const int32 SlotIndex = ChunkStore->AcquireWorkerLease(Coordinate);
	check(SlotIndex != INDEX_NONE);

	FVoxelNeighborChunks Neighbors;
	if (ChunkStore)
	{
		Neighbors.NegX = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X - 1, Coordinate.Y, Coordinate.Z));
		Neighbors.PosX = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X + 1, Coordinate.Y, Coordinate.Z));
		Neighbors.NegY = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y - 1, Coordinate.Z));
		Neighbors.PosY = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y + 1, Coordinate.Z));
		Neighbors.NegZ = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z - 1));
		Neighbors.PosZ = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z + 1));
	}

	const int32 CapturedSeed = WorldSeed;
	const int32 CapturedChunkSize = ChunkSize;
	const UVoxelBlockRegistry* CapturedRegistry = BlockRegistry;
	const float CapturedVoxelWorldSize = VoxelWorldSize;
	TArray<const UVoxelBiomeDefinition*> CapturedBiomes = AvailableBiomes;
	TSharedPtr<FVoxelGenerationPipeline> CapturedPipeline = GenerationPipeline;
	TWeakObjectPtr<UVoxelWorldSubsystem> WeakThis(this);

	TSharedRef<FVoxelMeshData, ESPMode::ThreadSafe> MeshDataResult = MakeShared<FVoxelMeshData, ESPMode::ThreadSafe>();
	TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> CancelFlag = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	InFlightCancelFlags.Add(Coordinate, CancelFlag);

	const FVoxelJobHandle JobHandle = FVoxelRuntimeModule::Get().GetScheduler().Submit(
		[Chunk, CapturedPipeline, CapturedSeed, Coordinate, CapturedChunkSize, CapturedRegistry, CapturedBiomes, Neighbors, CapturedVoxelWorldSize, MeshDataResult, CancelFlag]()
		{
			if (CancelFlag->Load())
			{
				return;
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerGenerate);
				if (CapturedPipeline)
				{
					CapturedPipeline->GenerateChunk(CapturedSeed, Coordinate, CapturedChunkSize, CapturedRegistry, CapturedBiomes, *Chunk);
				}
			}

			if (CancelFlag->Load())
			{
				return;
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerMesh);
				*MeshDataResult = FVoxelMesher::GenerateMesh(*Chunk, CapturedRegistry, &Neighbors, &Coordinate, CapturedVoxelWorldSize);
			}
		},
		WorkPriority,
		[WeakThis, Coordinate, MeshDataResult, SlotIndex]()
		{
			if (UVoxelWorldSubsystem* StrongThis = WeakThis.Get())
			{
				StrongThis->CompletedMeshQueue.Enqueue(FVoxelCompletedMeshItem(
					Coordinate,
					SlotIndex,
					MoveTemp(*MeshDataResult),
					FPlatformTime::Seconds()
				));
				StrongThis->FinalizationQueueDepth++;
			}
		});

	InFlightJobHandles.Add(Coordinate, JobHandle);

	return ExistingHandle;
}

void UVoxelWorldSubsystem::RequestRemeshChunk(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority)
{
	check(IsInGameThread());
	if (!ReadyCoordinates.Contains(Coordinate) || InFlightJobHandles.Contains(Coordinate) || PendingRemeshCoordinates.Contains(Coordinate) || !ChunkStore)
	{
		return;
	}

	const FVoxelChunk* Chunk = ChunkStore->FindChunkByCoordinate(Coordinate);
	if (!Chunk || Chunk->IsEmpty())
	{
		return;
	}

	const int32 SlotIndex = ChunkStore->AcquireWorkerLease(Coordinate);
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}

	PendingRemeshCoordinates.Add(Coordinate);

	FVoxelNeighborChunks Neighbors;
	Neighbors.NegX = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X - 1, Coordinate.Y, Coordinate.Z));
	Neighbors.PosX = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X + 1, Coordinate.Y, Coordinate.Z));
	Neighbors.NegY = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y - 1, Coordinate.Z));
	Neighbors.PosY = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y + 1, Coordinate.Z));
	Neighbors.NegZ = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z - 1));
	Neighbors.PosZ = ChunkStore->FindChunkByCoordinate(FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z + 1));

	const UVoxelBlockRegistry* CapturedRegistry = BlockRegistry;
	const float CapturedVoxelWorldSize = VoxelWorldSize;
	TWeakObjectPtr<UVoxelWorldSubsystem> WeakThis(this);

	TSharedRef<FVoxelMeshData, ESPMode::ThreadSafe> MeshDataResult = MakeShared<FVoxelMeshData, ESPMode::ThreadSafe>();
	TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> CancelFlag = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	InFlightCancelFlags.Add(Coordinate, CancelFlag);

	const FVoxelJobHandle JobHandle = FVoxelRuntimeModule::Get().GetScheduler().Submit(
		[Chunk, CapturedRegistry, Neighbors, Coordinate, CapturedVoxelWorldSize, MeshDataResult, CancelFlag]()
		{
			if (CancelFlag->Load())
			{
				return;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerMesh);
			*MeshDataResult = FVoxelMesher::GenerateMesh(*Chunk, CapturedRegistry, &Neighbors, &Coordinate, CapturedVoxelWorldSize);
		},
		WorkPriority,
		[WeakThis, Coordinate, MeshDataResult, SlotIndex]()
		{
			if (UVoxelWorldSubsystem* StrongThis = WeakThis.Get())
			{
				StrongThis->CompletedMeshQueue.Enqueue(FVoxelCompletedMeshItem(
					Coordinate,
					SlotIndex,
					MoveTemp(*MeshDataResult),
					FPlatformTime::Seconds(),
					/*bInIsRemesh=*/true
				));
				StrongThis->FinalizationQueueDepth++;
			}
		});

	InFlightJobHandles.Add(Coordinate, JobHandle);
}

UVoxelMeshComponent* UVoxelWorldSubsystem::GetOrCreateMeshComponent(const FVoxelChunkCoordinate& Coordinate)
{
	check(IsInGameThread());

	if (TWeakObjectPtr<UVoxelMeshComponent>* Existing = ChunkMeshComponents.Find(Coordinate))
	{
		if (Existing->IsValid())
		{
			return Existing->Get();
		}
	}

	if (!RenderHostActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		RenderHostActor = GetWorld()->SpawnActor<AVoxelWorldRenderActor>(SpawnParams);
	}

	UVoxelMeshComponent* Component = nullptr;

	// Check if an idle component is available in the pool
	while (ComponentPool.Num() > 0)
	{
		TObjectPtr<UVoxelMeshComponent> PooledComp = ComponentPool.Pop(EAllowShrinking::No);
		if (IsValid(PooledComp))
		{
			Component = PooledComp;
			ReusedComponentCount++;
			break;
		}
	}

	if (!Component)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ComponentRegistration);
		Component = NewObject<UVoxelMeshComponent>(RenderHostActor,
			*FString::Printf(TEXT("ChunkMesh_%d_%d_%d"), Coordinate.X, Coordinate.Y, Coordinate.Z));
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetupAttachment(RenderHostActor->GetRootComponent());
		Component->RegisterComponent();
		CreatedComponentCount++;
	}

	Component->SetCastShadow(bCastShadows);
	Component->SetVisibility(true, /*bPropagateToChildren=*/ true);

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

void UVoxelWorldSubsystem::UnloadChunk(const FVoxelChunkCoordinate& Coordinate, bool bTriggerNeighborRemesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_UnloadChunk);
	check(IsInGameThread());

	PendingRemeshCoordinates.Remove(Coordinate);

	if (TWeakObjectPtr<UVoxelMeshComponent>* ComponentPtr = ChunkMeshComponents.Find(Coordinate))
	{
		if (ComponentPtr->IsValid())
		{
			UVoxelMeshComponent* Comp = ComponentPtr->Get();
			Comp->ClearMeshData();
			Comp->SetVisibility(false, /*bPropagateToChildren=*/ true);

			if (ComponentPool.Num() < MaxComponentPoolSize)
			{
				ComponentPool.Add(Comp);
				PeakPoolSize = FMath::Max(PeakPoolSize, ComponentPool.Num());
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_ComponentDestruction);
				Comp->DestroyComponent();
				DestroyedComponentCount++;
			}
		}
		ChunkMeshComponents.Remove(Coordinate);
	}

	ChunkStates.Add(Coordinate, EVoxelChunkState::Unloading);
	RequestedCoordinates.Remove(Coordinate);
	ReadyCoordinates.Remove(Coordinate);

	if (TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe>* FoundCancel = InFlightCancelFlags.Find(Coordinate))
	{
		(*FoundCancel)->Store(true);
		InFlightCancelFlags.Remove(Coordinate);
	}

	if (const FVoxelJobHandle* JobHandle = InFlightJobHandles.Find(Coordinate))
	{
		FVoxelRuntimeModule::Get().GetScheduler().RequestCancel(*JobHandle);
		InFlightJobHandles.Remove(Coordinate);
	}

	if (ChunkStore)
	{
		ChunkStore->RemoveChunk(Coordinate);
	}

	// Neighbor Unload Remesh Trigger: check 6 cardinal neighbors - if resident & ready, remesh to restore boundary faces
	if (bTriggerNeighborRemesh)
	{
		const FVoxelChunkCoordinate CardinalNeighbors[6] = {
			FVoxelChunkCoordinate(Coordinate.X - 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X + 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y - 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y + 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z - 1),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z + 1)
		};

		for (const FVoxelChunkCoordinate& NeighborCoord : CardinalNeighbors)
		{
			if (IsChunkReady(NeighborCoord) && !InFlightJobHandles.Contains(NeighborCoord) && !PendingRemeshCoordinates.Contains(NeighborCoord))
			{
				RequestRemeshChunk(NeighborCoord, EVoxelWorkPriority::Low);
			}
		}
	}
}

void UVoxelWorldSubsystem::SetCastShadows(bool bInCastShadows)
{
	bCastShadows = bInCastShadows;
	for (const TPair<FVoxelChunkCoordinate, TWeakObjectPtr<UVoxelMeshComponent>>& Pair : ChunkMeshComponents)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->SetCastShadow(bCastShadows);
		}
	}
}

void UVoxelWorldSubsystem::ClearAllChunks()
{
	check(IsInGameThread());
	for (auto& Pair : InFlightCancelFlags)
	{
		Pair.Value->Store(true);
	}
	InFlightCancelFlags.Empty();
	InFlightJobHandles.Empty();
	PendingRemeshCoordinates.Empty();

	TArray<FVoxelChunkCoordinate> CoordsToUnload;
	CoordsToUnload.Reserve(ChunkStates.Num());
	for (const TPair<FVoxelChunkCoordinate, EVoxelChunkState>& Pair : ChunkStates)
	{
		CoordsToUnload.Add(Pair.Key);
	}
	for (const FVoxelChunkCoordinate& Coord : CoordsToUnload)
	{
		UnloadChunk(Coord, /*bTriggerNeighborRemesh=*/false);
	}
	FVoxelCompletedMeshItem DroppedItem;
	while (CompletedMeshQueue.Dequeue(DroppedItem))
	{
		if (ChunkStore && DroppedItem.SlotIndex != INDEX_NONE)
		{
			ChunkStore->ReleaseWorkerLease(DroppedItem.SlotIndex);
		}
	}
	FinalizationQueueDepth = 0;
}

const FVoxelChunk* UVoxelWorldSubsystem::FindChunk(const FVoxelChunkCoordinate& Coordinate) const
{
	return ChunkStore ? ChunkStore->FindChunkByCoordinate(Coordinate) : nullptr;
}

bool UVoxelWorldSubsystem::IsChunkReady(const FVoxelChunkCoordinate& Coordinate) const
{
	return ReadyCoordinates.Contains(Coordinate);
}

EVoxelChunkState UVoxelWorldSubsystem::GetChunkState(const FVoxelChunkCoordinate& Coordinate) const
{
	if (const EVoxelChunkState* Found = ChunkStates.Find(Coordinate))
	{
		return *Found;
	}
	return EVoxelChunkState::Unloaded;
}

void UVoxelWorldSubsystem::SetChunkVisible(const FVoxelChunkCoordinate& Coordinate, bool bVisible)
{
	check(IsInGameThread());

	if (const TWeakObjectPtr<UVoxelMeshComponent>* WeakComp = ChunkMeshComponents.Find(Coordinate))
	{
		if (WeakComp->IsValid())
		{
			UVoxelMeshComponent* Comp = WeakComp->Get();
			if (Comp->IsVisible() != bVisible)
			{
				Comp->SetVisibility(bVisible, /*bPropagateToChildren=*/ true);
			}
		}
	}
}
