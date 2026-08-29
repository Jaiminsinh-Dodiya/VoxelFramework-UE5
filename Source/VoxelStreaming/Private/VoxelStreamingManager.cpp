// VoxelStreamingManager.cpp

#include "VoxelStreamingManager.h"
#include "VoxelStreamingTypes.h"
#include "VoxelWorldSubsystem.h"
#include "VoxelRuntimeSettings.h"
#include "VoxelWorldSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelStreaming, Log, All);

void UVoxelStreamingManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Depend on VoxelWorldSubsystem being initialized first.
	Collection.InitializeDependency<UVoxelWorldSubsystem>();
	WorldSubsystem = GetWorld()->GetSubsystem<UVoxelWorldSubsystem>();

	if (!WorldSubsystem)
	{
		UE_LOG(LogVoxelStreaming, Error, TEXT("UVoxelWorldSubsystem not found - streaming manager will be inert."));
		return;
	}

	const UVoxelRuntimeSettings* RuntimeSettings = GetDefault<UVoxelRuntimeSettings>();
	ChunkSize = RuntimeSettings->ChunkSize;
	WorldHeightInChunks = RuntimeSettings->WorldHeightInChunks;
	SimulationDistance = RuntimeSettings->SimulationDistance;
	RenderDistance = RuntimeSettings->RenderDistance;
	GenerationDistance = RuntimeSettings->GenerationDistance;
	PersistenceDistance = RuntimeSettings->PersistenceDistance;
	StreamingBudgetMs = RuntimeSettings->StreamingBudgetMs;

	const UVoxelWorldSettings* WorldSettings = GetDefault<UVoxelWorldSettings>();
	VoxelWorldSize = WorldSettings->VoxelWorldSize;

	ChunkWorldEdgeSize = ChunkSize * VoxelWorldSize;
	InvChunkWorldEdgeSize = (ChunkWorldEdgeSize > 0.0f) ? (1.0f / ChunkWorldEdgeSize) : 0.0f;

	RebuildCachedOffsets();

	UE_LOG(LogVoxelStreaming, Log,
		TEXT("Initialized: Sim=%d Render=%d Gen=%d Persist=%d Budget=%.1fms Height=%d CachedOffsets=%d"),
		SimulationDistance, RenderDistance, GenerationDistance, PersistenceDistance,
		StreamingBudgetMs, WorldHeightInChunks, CachedRelativeOffsets.Num());
}

void UVoxelStreamingManager::RebuildCachedOffsets()
{
	CachedRelativeOffsets.Reset();
	const int32 Side = 2 * GenerationDistance + 1;
	const int32 MaxDZ = FMath::Min(WorldHeightInChunks - 1, GenerationDistance);
	CachedRelativeOffsets.Reserve(Side * Side * (2 * MaxDZ + 1));

	for (int32 DX = -GenerationDistance; DX <= GenerationDistance; ++DX)
	{
		for (int32 DY = -GenerationDistance; DY <= GenerationDistance; ++DY)
		{
			for (int32 DZ = -MaxDZ; DZ <= MaxDZ; ++DZ)
			{
				const int32 ChebyshevDist = FMath::Max(FMath::Abs(DX), FMath::Max(FMath::Abs(DY), FMath::Abs(DZ)));
				if (ChebyshevDist <= GenerationDistance)
				{
					CachedRelativeOffsets.Add(FIntVector(DX, DY, DZ));
				}
			}
		}
	}

	// Sort ascending by Chebyshev distance ONCE at cache build time
	CachedRelativeOffsets.Sort([](const FIntVector& A, const FIntVector& B)
	{
		const int32 DistA = FMath::Max(FMath::Abs(A.X), FMath::Max(FMath::Abs(A.Y), FMath::Abs(A.Z)));
		const int32 DistB = FMath::Max(FMath::Abs(B.X), FMath::Max(FMath::Abs(B.Y), FMath::Abs(B.Z)));
		return DistA < DistB;
	});
}

void UVoxelStreamingManager::Deinitialize()
{
	ManagedCoordinates.Reset();
	VisibleCoordinates.Reset();
	PendingRequests.Reset();
	PendingRequestIndex = 0;
	PendingUnloads.Reset();
	PendingUnloadIndex = 0;
	CachedRelativeOffsets.Reset();
	WorldSubsystem = nullptr;

	Super::Deinitialize();
}

TStatId UVoxelStreamingManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVoxelStreamingManager, STATGROUP_Tickables);
}

void UVoxelStreamingManager::SetViewerPosition(const FVector& WorldPosition)
{
	if (WorldPosition.X == FLT_MAX)
	{
		bUseManualViewer = false;
		return;
	}
	ManualViewerPosition = WorldPosition;
	bUseManualViewer = true;
}

FVector UVoxelStreamingManager::GetAutoViewerPosition() const
{
	UWorld* World = GetWorld();
	if (!World) return FVector::ZeroVector;

	// Try first local player's pawn, fall back to camera.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			return Pawn->GetActorLocation();
		}
		// No pawn — use camera location (spectator, etc.)
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		return CamLoc;
	}
	return FVector::ZeroVector;
}

FVoxelChunkCoordinate UVoxelStreamingManager::WorldToChunkCoordinate(const FVector& WorldPosition) const
{
	return FVoxelChunkCoordinate(
		FMath::FloorToInt32(WorldPosition.X * InvChunkWorldEdgeSize),
		FMath::FloorToInt32(WorldPosition.Y * InvChunkWorldEdgeSize),
		FMath::Clamp(FMath::FloorToInt32(WorldPosition.Z * InvChunkWorldEdgeSize), 0, WorldHeightInChunks - 1));
}

void UVoxelStreamingManager::ClearAllManaged()
{
	ManagedCoordinates.Reset();
	VisibleCoordinates.Reset();
	PendingRequests.Reset();
	PendingRequestIndex = 0;
	PendingUnloads.Reset();
	PendingUnloadIndex = 0;
	LastViewerChunk = FVoxelChunkCoordinate(INT32_MAX, INT32_MAX, INT32_MAX);
	bFirstTick = true;
}

void UVoxelStreamingManager::SetRenderDistance(int32 InRenderDistance)
{
	InRenderDistance = FMath::Max(1, InRenderDistance);
	if (RenderDistance != InRenderDistance)
	{
		RenderDistance = InRenderDistance;
		GenerationDistance = FMath::Max(GenerationDistance, RenderDistance);
		PersistenceDistance = FMath::Max(PersistenceDistance, GenerationDistance);
		RebuildCachedOffsets();
		bForceQueueReevaluation = true;
	}
}

void UVoxelStreamingManager::SetSimulationDistance(int32 InSimulationDistance)
{
	InSimulationDistance = FMath::Clamp(InSimulationDistance, 0, RenderDistance);
	if (SimulationDistance != InSimulationDistance)
	{
		SimulationDistance = InSimulationDistance;
		bForceQueueReevaluation = true;
	}
}

void UVoxelStreamingManager::SetGenerationDistance(int32 InGenerationDistance)
{
	InGenerationDistance = FMath::Max(RenderDistance, InGenerationDistance);
	if (GenerationDistance != InGenerationDistance)
	{
		GenerationDistance = InGenerationDistance;
		PersistenceDistance = FMath::Max(PersistenceDistance, GenerationDistance);
		RebuildCachedOffsets();
		bForceQueueReevaluation = true;
	}
}

void UVoxelStreamingManager::SetPersistenceDistance(int32 InPersistenceDistance)
{
	InPersistenceDistance = FMath::Max(GenerationDistance, InPersistenceDistance);
	if (PersistenceDistance != InPersistenceDistance)
	{
		PersistenceDistance = InPersistenceDistance;
		bForceQueueReevaluation = true;
	}
}

void UVoxelStreamingManager::SetStreamingBudgetMs(float InBudgetMs)
{
	StreamingBudgetMs = FMath::Max(0.1f, InBudgetMs);
}

EVoxelWorkPriority UVoxelStreamingManager::GetPriorityForCoordinate(const FVoxelChunkCoordinate& Coordinate, const FVoxelChunkCoordinate& ViewerChunk) const
{
	return GetPriorityForDistance(ViewerChunk.ChebyshevDistanceTo(Coordinate));
}

void UVoxelStreamingManager::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_StreamingTick);

	if (!WorldSubsystem) return;

	const FVector ViewerWorld = bUseManualViewer ? ManualViewerPosition : GetAutoViewerPosition();
	const FVoxelChunkCoordinate ViewerChunk = WorldToChunkCoordinate(ViewerWorld);

	// In frozen mode (Mode D), keep existing resident chunks
	if (bStreamingFrozen)
	{
		LastTickBudgetUsedMs = 0.0f;
		return;
	}

	const bool bViewerMoved = !(ViewerChunk == LastViewerChunk);

	// --- Recompute desired/unload sets when viewer moves or settings change ---
	if (bFirstTick || bViewerMoved || bForceQueueReevaluation)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_QueueEvaluation);
		LastViewerChunk = ViewerChunk;
		bFirstTick = false;
		bForceQueueReevaluation = false;

		// 1. Build new request list from pre-sorted relative offsets in O(N) with zero heap reallocations.
		PendingRequests.Reset();
		PendingRequestIndex = 0;
		for (const FIntVector& Offset : CachedRelativeOffsets)
		{
			const int32 CZ = ViewerChunk.Z + Offset.Z;
			if (CZ >= 0 && CZ < WorldHeightInChunks)
			{
				const FVoxelChunkCoordinate Candidate(ViewerChunk.X + Offset.X, ViewerChunk.Y + Offset.Y, CZ);
				if (!ManagedCoordinates.Contains(Candidate))
				{
					PendingRequests.Add(Candidate);
				}
			}
		}

		// 2. Merged single-pass over ManagedCoordinates for Unloads + Visibility (eliminates redundant pass & duplicate distance work).
		PendingUnloads.Reset();
		PendingUnloadIndex = 0;
		for (const FVoxelChunkCoordinate& Coord : ManagedCoordinates)
		{
			const int32 Dist = ViewerChunk.ChebyshevDistanceTo(Coord);
			const EVoxelStreamingBand Band = VoxelStreaming::ClassifyChunkDistance(
				Dist, SimulationDistance, RenderDistance, GenerationDistance, PersistenceDistance);

			if (Band == EVoxelStreamingBand::OutOfRange)
			{
				PendingUnloads.Add(Coord);
			}

			const bool bShouldBeVisible = (Band <= EVoxelStreamingBand::Render);
			const bool bIsCurrentlyVisible = VisibleCoordinates.Contains(Coord);
			if (bShouldBeVisible != bIsCurrentlyVisible)
			{
				if (bShouldBeVisible)
				{
					VisibleCoordinates.Add(Coord);
				}
				else
				{
					VisibleCoordinates.Remove(Coord);
				}
				WorldSubsystem->SetChunkVisible(Coord, bShouldBeVisible);
			}
		}

		UE_LOG(LogVoxelStreaming, Verbose, TEXT("Viewer (%d,%d,%d): %d new requests, %d unloads queued"),
			ViewerChunk.X, ViewerChunk.Y, ViewerChunk.Z, PendingRequests.Num(), PendingUnloads.Num());
	}

	// --- Budget-limited processing (Adaptive Streaming Budget) ---
	float EffectiveBudgetMs = StreamingBudgetMs;
	if (DeltaTime > 0.0333f) // Frame taking >33.3ms (missing 30 FPS target) - back off heavily to recover frame pacing
	{
		EffectiveBudgetMs = FMath::Max(0.1f, StreamingBudgetMs * 0.2f);
	}
	else if (DeltaTime > 0.0167f) // Frame taking >16.7ms (missing 60 FPS target) - throttle down moderately
	{
		EffectiveBudgetMs = FMath::Max(0.3f, StreamingBudgetMs * 0.5f);
	}

	const double BudgetStart = FPlatformTime::Seconds();
	const double BudgetEnd = BudgetStart + EffectiveBudgetMs * 0.001;

	// Process requests first (loading is more important than unloading).
	int32 RequestLoopCount = 0;
	while (PendingRequestIndex < PendingRequests.Num())
	{
		// Poll FPlatformTime every 8 iterations to reduce clock syscall overhead
		if ((RequestLoopCount++ & 0x7) == 0 && FPlatformTime::Seconds() >= BudgetEnd)
		{
			break;
		}

		const FVoxelChunkCoordinate Coord = PendingRequests[PendingRequestIndex++];

		if (!ManagedCoordinates.Contains(Coord)) // double-check in case of overlap with prior tick
		{
			const int32 Dist = ViewerChunk.ChebyshevDistanceTo(Coord);
			const EVoxelWorkPriority Priority = GetPriorityForDistance(Dist);
			WorldSubsystem->RequestChunk(Coord, Priority);
			ManagedCoordinates.Add(Coord);

			// If already within RenderDistance, register desired visibility
			if (Dist <= RenderDistance)
			{
				VisibleCoordinates.Add(Coord);
			}
		}
	}
	if (PendingRequestIndex >= PendingRequests.Num())
	{
		PendingRequests.Reset();
		PendingRequestIndex = 0;
	}

	// Process unloads with remaining budget.
	int32 UnloadLoopCount = 0;
	while (PendingUnloadIndex < PendingUnloads.Num())
	{
		// Poll FPlatformTime every 8 iterations
		if ((UnloadLoopCount++ & 0x7) == 0 && FPlatformTime::Seconds() >= BudgetEnd)
		{
			break;
		}

		const FVoxelChunkCoordinate Coord = PendingUnloads[PendingUnloadIndex++];

		if (ManagedCoordinates.Contains(Coord))
		{
			VisibleCoordinates.Remove(Coord);
			WorldSubsystem->UnloadChunk(Coord);
			ManagedCoordinates.Remove(Coord);
		}
	}
	if (PendingUnloadIndex >= PendingUnloads.Num())
	{
		PendingUnloads.Reset();
		PendingUnloadIndex = 0;
	}

	LastTickBudgetUsedMs = static_cast<float>((FPlatformTime::Seconds() - BudgetStart) * 1000.0);

	const int32 RemainingRequests = PendingRequests.Num() - PendingRequestIndex;
	const int32 RemainingUnloads = PendingUnloads.Num() - PendingUnloadIndex;
	if (RemainingRequests > 0 || RemainingUnloads > 0)
	{
		UE_LOG(LogVoxelStreaming, Verbose, TEXT("Budget exhausted: %d requests, %d unloads remaining (used %.2fms)"),
			RemainingRequests, RemainingUnloads, LastTickBudgetUsedMs);
	}
}
