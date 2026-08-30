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
#include "VoxelCollisionComponent.h"
#include "VoxelCollisionBuilder.h"
#include "VoxelPhysicsTypes.h"
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
	// 1. Flag in-flight cancellation on all active jobs
	for (auto& Pair : InFlightCancelFlags)
	{
		Pair.Value->Store(true);
	}
	for (const auto& Pair : InFlightJobHandles)
	{
		if (FVoxelRuntimeModule::IsAvailable())
		{
			FVoxelRuntimeModule::Get().GetScheduler().RequestCancel(Pair.Value);
		}
	}

	// 2. World shutdown barrier: wait for all in-flight workers to exit before resetting storage memory
	if (FVoxelRuntimeModule::IsAvailable())
	{
		const bool bAllTasksFinished = FVoxelRuntimeModule::Get().GetScheduler().WaitForAllTasks(5.0);
		if (!bAllTasksFinished)
		{
			UE_LOG(LogVoxelWorld, Error,
				TEXT("UVoxelWorldSubsystem::Deinitialize: Background worker tasks did not finish within timeout! Preserving ChunkStore to prevent memory corruption."));
			Super::Deinitialize();
			return;
		}
	}

	// 3. All background tasks have exited. Safely unload chunks & drain completion queue.
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
	ProcessCompletedCollisionQueue(DeltaTime);
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

	// Accurate Oldest Pending Age via Queue Peek
	const FVoxelCompletedMeshItem* PeekItem = CompletedMeshQueue.Peek();
	OldestQueueItemAgeMs = PeekItem ? static_cast<float>((FPlatformTime::Seconds() - PeekItem->QueueEntryTime) * 1000.0) : 0.0f;

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

void UVoxelWorldSubsystem::ResetLatencyStats()
{
	QueueLatencyHistory.Reset();
	AverageQueueLatencyMs = 0.0f;
	MaxQueueLatencyMs = 0.0f;
	OldestQueueItemAgeMs = 0.0f;
	TotalQueueLatencyAccumMs = 0.0;
	TotalFinalizedItemsSampled = 0;
}

void UVoxelWorldSubsystem::ProcessCompletedCollisionQueue(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GTFinalizeCollision);

	if (FinalizationCollisionQueueDepth.Load() <= 0)
	{
		return;
	}

	const double StartSec = FPlatformTime::Seconds();
	const double BudgetSec = RenderSubmissionBudgetMs * 0.001;
	const int32 MaxItemsPerTick = 4;
	int32 ProcessedCount = 0;

	FVoxelCompletedCollisionItem Item;
	while (CompletedCollisionQueue.Dequeue(Item))
	{
		FinalizationCollisionQueueDepth--;
		FinalizeChunkCollision(MoveTemp(Item));
		ProcessedCount++;

		if ((FPlatformTime::Seconds() - StartSec) >= BudgetSec || ProcessedCount >= MaxItemsPerTick)
		{
			break;
		}
	}
}

void UVoxelWorldSubsystem::FinalizeChunkCollision(FVoxelCompletedCollisionItem&& Item)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionUpload);
	check(IsInGameThread());

	const FVoxelChunkCoordinate Coordinate = Item.Coordinate;
	InFlightCollisionJobHandles.Remove(Coordinate);
	InFlightCollisionCancelFlags.Remove(Coordinate);

	// Stale / unload verification
	const uint32 ActiveRevision = ChunkCollisionRevisions.FindRef(Coordinate);
	if (!RequestedCoordinates.Contains(Coordinate) || Item.CollisionRevision != ActiveRevision)
	{
		if (ChunkStore)
		{
			if (Item.SlotIndex != INDEX_NONE)
			{
				ChunkStore->ReleaseWorkerLease(Item.SlotIndex);
			}
			for (int32 NSlot : Item.NeighborSlotIndices)
			{
				ChunkStore->ReleaseWorkerLease(NSlot);
			}
		}
		return;
	}

	if (Item.CollisionData.IsEmpty())
	{
		CollisionStates.Add(Coordinate, EVoxelCollisionState::Ready);
		if (TWeakObjectPtr<UVoxelCollisionComponent>* ExistingComp = ChunkCollisionComponents.Find(Coordinate))
		{
			if (ExistingComp->IsValid())
			{
				ExistingComp->Get()->ClearCollisionData();
			}
		}
	}
	else
	{
		UVoxelCollisionComponent* Component = GetOrCreateCollisionComponent(Coordinate);
		Component->OnCollisionCookFinished.RemoveAll(this);
		Component->OnCollisionCookFinished.AddUObject(this, &UVoxelWorldSubsystem::HandleCollisionCookFinished, Coordinate);
		Component->SetCollisionData(MoveTemp(Item.CollisionData), /*bAsyncCook=*/ true);
		CollisionStates.Add(Coordinate, EVoxelCollisionState::Cooking);
	}

	if (ChunkStore)
	{
		if (Item.SlotIndex != INDEX_NONE)
		{
			ChunkStore->ReleaseWorkerLease(Item.SlotIndex);
		}
		for (int32 NSlot : Item.NeighborSlotIndices)
		{
			ChunkStore->ReleaseWorkerLease(NSlot);
		}
	}
}

void UVoxelWorldSubsystem::HandleCollisionCookFinished(UVoxelCollisionComponent* Comp, bool bSuccess, uint32 Revision, FVoxelChunkCoordinate Coordinate)
{
	check(IsInGameThread());

	// If chunk was unloaded, cancelled, or revision superseded while cooking was in-flight, discard
	if (!RequestedCoordinates.Contains(Coordinate) || ChunkCollisionRevisions.FindRef(Coordinate) != Revision)
	{
		return;
	}

	if (bSuccess)
	{
		CollisionStates.Add(Coordinate, EVoxelCollisionState::Ready);
	}
	else
	{
		CollisionStates.Add(Coordinate, EVoxelCollisionState::Failed);
	}
}

void UVoxelWorldSubsystem::FinalizeChunkMesh(FVoxelCompletedMeshItem&& Item)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_MeshUpload);
	check(IsInGameThread());

	const FVoxelChunkCoordinate Coordinate = Item.Coordinate;
	InFlightJobHandles.Remove(Coordinate);
	InFlightCancelFlags.Remove(Coordinate);
	PendingRemeshCoordinates.Remove(Coordinate);

	// Guard against unloads while work was in flight or queued (Stale Result Protection)
	if (!RequestedCoordinates.Contains(Coordinate))
	{
		ChunkStates.Remove(Coordinate);
		if (ChunkStore)
		{
			if (Item.SlotIndex != INDEX_NONE)
			{
				ChunkStore->ReleaseWorkerLease(Item.SlotIndex);
			}
			for (int32 NSlot : Item.NeighborSlotIndices)
			{
				ChunkStore->ReleaseWorkerLease(NSlot);
			}
		}
		return;
	}

	ReadyCoordinates.Add(Coordinate);
	ChunkStates.Add(Coordinate, EVoxelChunkState::Ready);

	// If collision was queued waiting for generation to finish, kick off collision build now
	if (GetChunkCollisionState(Coordinate) == EVoxelCollisionState::Queued)
	{
		RequestChunkCollision(Coordinate, EVoxelWorkPriority::High);
	}

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

	if (ChunkStore)
	{
		if (Item.SlotIndex != INDEX_NONE)
		{
			ChunkStore->ReleaseWorkerLease(Item.SlotIndex);
		}
		for (int32 NSlot : Item.NeighborSlotIndices)
		{
			ChunkStore->ReleaseWorkerLease(NSlot);
		}
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
	TArray<int32> NeighborSlotIndices;

	if (ChunkStore)
	{
		const FVoxelChunkCoordinate CardinalCoords[6] = {
			FVoxelChunkCoordinate(Coordinate.X - 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X + 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y - 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y + 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z - 1),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z + 1)
		};

		const FVoxelChunk** NeighborPtrs[6] = {
			&Neighbors.NegX, &Neighbors.PosX,
			&Neighbors.NegY, &Neighbors.PosY,
			&Neighbors.NegZ, &Neighbors.PosZ
		};

		for (int32 i = 0; i < 6; ++i)
		{
			const FVoxelChunkCoordinate& NCoord = CardinalCoords[i];
			// Invariant: Only Ready neighbors are readable. Unready/generating/unloaded neighbors are treated as air.
			if (IsChunkReady(NCoord))
			{
				const int32 NSlot = ChunkStore->AcquireWorkerLease(NCoord);
				if (NSlot != INDEX_NONE)
				{
					*NeighborPtrs[i] = ChunkStore->FindChunkByCoordinate(NCoord);
					NeighborSlotIndices.Add(NSlot);
				}
			}
		}
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
		[WeakThis, Coordinate, MeshDataResult, SlotIndex, NeighborSlotIndices = MoveTemp(NeighborSlotIndices)]() mutable
		{
			if (UVoxelWorldSubsystem* StrongThis = WeakThis.Get())
			{
				StrongThis->CompletedMeshQueue.Enqueue(FVoxelCompletedMeshItem(
					Coordinate,
					SlotIndex,
					MoveTemp(NeighborSlotIndices),
					MoveTemp(*MeshDataResult),
					FPlatformTime::Seconds(),
					/*bInIsRemesh=*/false
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
	TArray<int32> NeighborSlotIndices;

	if (ChunkStore)
	{
		const FVoxelChunkCoordinate CardinalCoords[6] = {
			FVoxelChunkCoordinate(Coordinate.X - 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X + 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y - 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y + 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z - 1),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z + 1)
		};

		const FVoxelChunk** NeighborPtrs[6] = {
			&Neighbors.NegX, &Neighbors.PosX,
			&Neighbors.NegY, &Neighbors.PosY,
			&Neighbors.NegZ, &Neighbors.PosZ
		};

		for (int32 i = 0; i < 6; ++i)
		{
			const FVoxelChunkCoordinate& NCoord = CardinalCoords[i];
			// Invariant: Only Ready neighbors are readable. Unready/generating/unloaded neighbors are treated as air.
			if (IsChunkReady(NCoord))
			{
				const int32 NSlot = ChunkStore->AcquireWorkerLease(NCoord);
				if (NSlot != INDEX_NONE)
				{
					*NeighborPtrs[i] = ChunkStore->FindChunkByCoordinate(NCoord);
					NeighborSlotIndices.Add(NSlot);
				}
			}
		}
	}

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
		[WeakThis, Coordinate, MeshDataResult, SlotIndex, NeighborSlotIndices = MoveTemp(NeighborSlotIndices)]() mutable
		{
			if (UVoxelWorldSubsystem* StrongThis = WeakThis.Get())
			{
				StrongThis->CompletedMeshQueue.Enqueue(FVoxelCompletedMeshItem(
					Coordinate,
					SlotIndex,
					MoveTemp(NeighborSlotIndices),
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

	UnloadChunkCollision(Coordinate);

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

void UVoxelWorldSubsystem::RequestChunkCollision(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority)
{
	check(IsInGameThread());

	if (!ReadyCoordinates.Contains(Coordinate))
	{
		if (RequestedCoordinates.Contains(Coordinate))
		{
			CollisionStates.Add(Coordinate, EVoxelCollisionState::Queued);
		}
		return;
	}

	if (InFlightCollisionJobHandles.Contains(Coordinate) || !ChunkStore)
	{
		return;
	}

	const FVoxelChunk* Chunk = ChunkStore->FindChunkByCoordinate(Coordinate);
	if (!Chunk || Chunk->IsEmpty())
	{
		CollisionStates.Add(Coordinate, EVoxelCollisionState::Ready);
		return;
	}

	const int32 SlotIndex = ChunkStore->AcquireWorkerLease(Coordinate);
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}

	uint32& RevisionRef = ChunkCollisionRevisions.FindOrAdd(Coordinate);
	++RevisionRef;
	const uint32 CurrentRevision = RevisionRef;

	CollisionStates.Add(Coordinate, EVoxelCollisionState::Building);

	FVoxelNeighborChunks Neighbors;
	TArray<int32> NeighborSlotIndices;

	if (ChunkStore)
	{
		const FVoxelChunkCoordinate CardinalCoords[6] = {
			FVoxelChunkCoordinate(Coordinate.X - 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X + 1, Coordinate.Y, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y - 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y + 1, Coordinate.Z),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z - 1),
			FVoxelChunkCoordinate(Coordinate.X, Coordinate.Y, Coordinate.Z + 1)
		};

		const FVoxelChunk** NeighborPtrs[6] = {
			&Neighbors.NegX, &Neighbors.PosX,
			&Neighbors.NegY, &Neighbors.PosY,
			&Neighbors.NegZ, &Neighbors.PosZ
		};

		for (int32 i = 0; i < 6; ++i)
		{
			const FVoxelChunkCoordinate& NCoord = CardinalCoords[i];
			if (IsChunkReady(NCoord))
			{
				const int32 NSlot = ChunkStore->AcquireWorkerLease(NCoord);
				if (NSlot != INDEX_NONE)
				{
					*NeighborPtrs[i] = ChunkStore->FindChunkByCoordinate(NCoord);
					NeighborSlotIndices.Add(NSlot);
				}
			}
		}
	}

	const UVoxelBlockRegistry* CapturedRegistry = BlockRegistry;
	const float CapturedVoxelWorldSize = VoxelWorldSize;
	TWeakObjectPtr<UVoxelWorldSubsystem> WeakThis(this);

	TSharedRef<FVoxelCollisionData, ESPMode::ThreadSafe> CollisionDataResult = MakeShared<FVoxelCollisionData, ESPMode::ThreadSafe>();
	TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> CancelFlag = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	InFlightCollisionCancelFlags.Add(Coordinate, CancelFlag);

	const FVoxelJobHandle JobHandle = FVoxelRuntimeModule::Get().GetScheduler().Submit(
		[Chunk, CapturedRegistry, Neighbors, Coordinate, CapturedVoxelWorldSize, CurrentRevision, CollisionDataResult, CancelFlag]()
		{
			if (CancelFlag->Load())
			{
				return;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_WorkerCollisionBuild);
			*CollisionDataResult = FVoxelCollisionBuilder::BuildCollisionData(
				*Chunk, CapturedRegistry, &Neighbors, &Coordinate, CapturedVoxelWorldSize, CurrentRevision, EVoxelCollisionMode::Complex);
		},
		WorkPriority,
		[WeakThis, Coordinate, CollisionDataResult, SlotIndex, NeighborSlotIndices = MoveTemp(NeighborSlotIndices), CurrentRevision]() mutable
		{
			if (UVoxelWorldSubsystem* StrongThis = WeakThis.Get())
			{
				StrongThis->CompletedCollisionQueue.Enqueue(FVoxelCompletedCollisionItem(
					Coordinate,
					SlotIndex,
					MoveTemp(NeighborSlotIndices),
					MoveTemp(*CollisionDataResult),
					FPlatformTime::Seconds(),
					CurrentRevision
				));
				StrongThis->FinalizationCollisionQueueDepth++;
			}
		});

	InFlightCollisionJobHandles.Add(Coordinate, JobHandle);
}

void UVoxelWorldSubsystem::UnloadChunkCollision(const FVoxelChunkCoordinate& Coordinate)
{
	check(IsInGameThread());

	if (TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe>* CancelFlag = InFlightCollisionCancelFlags.Find(Coordinate))
	{
		(*CancelFlag)->Store(true);
	}
	InFlightCollisionCancelFlags.Remove(Coordinate);

	if (FVoxelJobHandle* JobHandle = InFlightCollisionJobHandles.Find(Coordinate))
	{
		if (FVoxelRuntimeModule::IsAvailable())
		{
			FVoxelRuntimeModule::Get().GetScheduler().RequestCancel(*JobHandle);
		}
	}
	InFlightCollisionJobHandles.Remove(Coordinate);

	if (TWeakObjectPtr<UVoxelCollisionComponent>* CompPtr = ChunkCollisionComponents.Find(Coordinate))
	{
		if (CompPtr->IsValid())
		{
			UVoxelCollisionComponent* Comp = CompPtr->Get();
			Comp->ClearCollisionData();
			Comp->DestroyComponent();
		}
		ChunkCollisionComponents.Remove(Coordinate);
	}

	CollisionStates.Remove(Coordinate);
	ChunkCollisionRevisions.Remove(Coordinate);
}

UVoxelCollisionComponent* UVoxelWorldSubsystem::GetOrCreateCollisionComponent(const FVoxelChunkCoordinate& Coordinate)
{
	check(IsInGameThread());

	if (TWeakObjectPtr<UVoxelCollisionComponent>* ExistingPtr = ChunkCollisionComponents.Find(Coordinate))
	{
		if (ExistingPtr->IsValid())
		{
			return ExistingPtr->Get();
		}
	}

	if (!RenderHostActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		RenderHostActor = GetWorld()->SpawnActor<AVoxelWorldRenderActor>(SpawnParams);
		check(RenderHostActor);
	}

	UVoxelCollisionComponent* Component = NewObject<UVoxelCollisionComponent>(RenderHostActor,
		*FString::Printf(TEXT("ChunkCollision_%d_%d_%d"), Coordinate.X, Coordinate.Y, Coordinate.Z));
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetRelativeLocation(FVector::ZeroVector);
	Component->SetupAttachment(RenderHostActor->GetRootComponent());
	Component->RegisterComponent();

	ChunkCollisionComponents.Add(Coordinate, Component);
	return Component;
}

EVoxelCollisionState UVoxelWorldSubsystem::GetChunkCollisionState(const FVoxelChunkCoordinate& Coordinate) const
{
	if (const EVoxelCollisionState* State = CollisionStates.Find(Coordinate))
	{
		return *State;
	}
	return EVoxelCollisionState::NotRequired;
}

bool UVoxelWorldSubsystem::HasChunkCollision(const FVoxelChunkCoordinate& Coordinate) const
{
	if (const TWeakObjectPtr<UVoxelCollisionComponent>* CompPtr = ChunkCollisionComponents.Find(Coordinate))
	{
		return CompPtr->IsValid() && CompPtr->Get()->HasActiveCollision();
	}
	return false;
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

	for (auto& Pair : InFlightCollisionCancelFlags)
	{
		Pair.Value->Store(true);
	}
	InFlightCollisionCancelFlags.Empty();
	InFlightCollisionJobHandles.Empty();

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

	// Drain mesh completions and release leases
	FVoxelCompletedMeshItem DroppedItem;
	while (CompletedMeshQueue.Dequeue(DroppedItem))
	{
		if (ChunkStore)
		{
			if (DroppedItem.SlotIndex != INDEX_NONE)
			{
				ChunkStore->ReleaseWorkerLease(DroppedItem.SlotIndex);
			}
			for (int32 NSlot : DroppedItem.NeighborSlotIndices)
			{
				ChunkStore->ReleaseWorkerLease(NSlot);
			}
		}
	}
	FinalizationQueueDepth = 0;

	// Drain collision completions and release leases
	FVoxelCompletedCollisionItem DroppedCollisionItem;
	while (CompletedCollisionQueue.Dequeue(DroppedCollisionItem))
	{
		if (ChunkStore)
		{
			if (DroppedCollisionItem.SlotIndex != INDEX_NONE)
			{
				ChunkStore->ReleaseWorkerLease(DroppedCollisionItem.SlotIndex);
			}
			for (int32 NSlot : DroppedCollisionItem.NeighborSlotIndices)
			{
				ChunkStore->ReleaseWorkerLease(NSlot);
			}
		}
	}
	FinalizationCollisionQueueDepth = 0;

	// Destroy any remaining collision components
	for (auto& Pair : ChunkCollisionComponents)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->ClearCollisionData();
			Pair.Value->DestroyComponent();
		}
	}
	ChunkCollisionComponents.Empty();
	CollisionStates.Empty();
	ChunkCollisionRevisions.Empty();
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
