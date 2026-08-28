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

	UE_LOG(LogVoxelStreaming, Log,
		TEXT("Initialized: Sim=%d Render=%d Gen=%d Persist=%d Budget=%.1fms Height=%d"),
		SimulationDistance, RenderDistance, GenerationDistance, PersistenceDistance,
		StreamingBudgetMs, WorldHeightInChunks);
}

void UVoxelStreamingManager::Deinitialize()
{
	ManagedCoordinates.Reset();
	PendingRequests.Reset();
	PendingUnloads.Reset();
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
	const float Scale = ChunkSize * VoxelWorldSize;
	return FVoxelChunkCoordinate(
		FMath::FloorToInt32(WorldPosition.X / Scale),
		FMath::FloorToInt32(WorldPosition.Y / Scale),
		FMath::Clamp(FMath::FloorToInt32(WorldPosition.Z / Scale), 0, WorldHeightInChunks - 1));
}

void UVoxelStreamingManager::Tick(float DeltaTime)
{
	if (!WorldSubsystem) return;

	const FVector ViewerWorld = bUseManualViewer ? ManualViewerPosition : GetAutoViewerPosition();
	const FVoxelChunkCoordinate ViewerChunk = WorldToChunkCoordinate(ViewerWorld);

	// --- Recompute desired/unload sets when viewer moves to a new chunk ---
	if (bFirstTick || !(ViewerChunk == LastViewerChunk))
	{
		LastViewerChunk = ViewerChunk;
		bFirstTick = false;

		// Compute the desired set (within GenerationDistance, Z-clamped, sorted by distance).
		const TArray<FVoxelChunkCoordinate> DesiredCoords =
			VoxelStreaming::ComputeDesiredCoordinates(ViewerChunk, GenerationDistance, WorldHeightInChunks);

		// Build new request list: desired coords that aren't already managed.
		const TSet<FVoxelChunkCoordinate> DesiredSet(DesiredCoords);
		PendingRequests.Reset();
		for (const FVoxelChunkCoordinate& Coord : DesiredCoords)
		{
			if (!ManagedCoordinates.Contains(Coord))
			{
				PendingRequests.Add(Coord); // already sorted by distance from ComputeDesiredCoordinates
			}
		}

		// Build unload list: managed coords that are now outside PersistenceDistance.
		PendingUnloads.Reset();
		for (const FVoxelChunkCoordinate& Coord : ManagedCoordinates)
		{
			if (ViewerChunk.ChebyshevDistanceTo(Coord) > PersistenceDistance)
			{
				PendingUnloads.Add(Coord);
			}
		}

		UE_LOG(LogVoxelStreaming, Verbose, TEXT("Viewer moved to (%d,%d,%d): %d new requests, %d unloads queued"),
			ViewerChunk.X, ViewerChunk.Y, ViewerChunk.Z, PendingRequests.Num(), PendingUnloads.Num());
	}

	// --- Budget-limited processing ---
	const double BudgetStart = FPlatformTime::Seconds();
	const double BudgetEnd = BudgetStart + StreamingBudgetMs * 0.001;

	// Process requests first (loading is more important than unloading).
	while (PendingRequests.Num() > 0 && FPlatformTime::Seconds() < BudgetEnd)
	{
		const FVoxelChunkCoordinate Coord = PendingRequests[0];
		PendingRequests.RemoveAt(0, EAllowShrinking::No);

		if (!ManagedCoordinates.Contains(Coord)) // double-check in case of overlap with prior tick
		{
			WorldSubsystem->RequestChunk(Coord);
			ManagedCoordinates.Add(Coord);
		}
	}

	// Process unloads with remaining budget.
	while (PendingUnloads.Num() > 0 && FPlatformTime::Seconds() < BudgetEnd)
	{
		const FVoxelChunkCoordinate Coord = PendingUnloads[0];
		PendingUnloads.RemoveAt(0, EAllowShrinking::No);

		if (ManagedCoordinates.Contains(Coord))
		{
			WorldSubsystem->UnloadChunk(Coord);
			ManagedCoordinates.Remove(Coord);
		}
	}

	// --- Visibility toggling (RenderDistance) ---
	// Cheap per-frame check: iterate managed coords, toggle visibility based
	// on whether they're within RenderDistance. SetVisibility is a no-op if
	// the state hasn't changed, so this doesn't trigger redundant work.
	// SetChunkVisible is a no-op if the chunk has no component (in-flight or all-air).
	for (const FVoxelChunkCoordinate& Coord : ManagedCoordinates)
	{
		const int32 Dist = ViewerChunk.ChebyshevDistanceTo(Coord);
		const EVoxelStreamingBand Band = VoxelStreaming::ClassifyChunkDistance(
			Dist, SimulationDistance, RenderDistance, GenerationDistance, PersistenceDistance);
		const bool bShouldBeVisible = (Band <= EVoxelStreamingBand::Render);

		WorldSubsystem->SetChunkVisible(Coord, bShouldBeVisible);
	}

	LastTickBudgetUsedMs = static_cast<float>((FPlatformTime::Seconds() - BudgetStart) * 1000.0);

	if (PendingRequests.Num() > 0 || PendingUnloads.Num() > 0)
	{
		UE_LOG(LogVoxelStreaming, Verbose, TEXT("Budget exhausted: %d requests, %d unloads remaining (used %.2fms)"),
			PendingRequests.Num(), PendingUnloads.Num(), LastTickBudgetUsedMs);
	}
}
