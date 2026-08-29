// VoxelStreamingManager.h
//
// Purpose:
//   Tick-driven streaming manager that decides which chunks to load and
//   unload based on a viewer position and the four distance bands defined
//   in UVoxelRuntimeSettings. Calls into UVoxelWorldSubsystem::RequestChunk
//   and UnloadChunk — does NOT touch FVoxelChunkStore, FVoxelGenerationPipeline,
//   or FVoxelMesher directly (scope boundary: this module decides WHEN and WHY,
//   VoxelWorld decides HOW).
//
// Responsibilities:
//   - Track a viewer position (auto-follows first local player, or set manually)
//   - Each tick: compute which chunk coordinates should be loaded (within
//     GenerationDistance), unloaded (outside PersistenceDistance), and visible
//     (within RenderDistance — toggle UVoxelMeshComponent::SetVisibility)
//   - Respect StreamingBudgetMs: bound how many Request/Unload calls per tick
//   - Z-axis clamped to [0, WorldHeightInChunks) — no wasted work on
//     guaranteed-all-air chunks above/below world height
//
// Thread ownership: Game Thread only (UTickableWorldSubsystem::Tick).
// Dependencies: VoxelCore, VoxelRuntime (settings), VoxelWorld (subsystem).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "VoxelCoreTypes.h"
#include "VoxelStreamingManager.generated.h"

class UVoxelWorldSubsystem;

UCLASS()
class VOXELSTREAMING_API UVoxelStreamingManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Override the auto-tracked viewer position. Pass FLT_MAX to re-enable auto-tracking. */
	void SetViewerPosition(const FVector& WorldPosition);

	/** Number of chunks currently managed by this streaming manager. */
	int32 GetManagedChunkCount() const { return ManagedCoordinates.Num(); }

	/** Number of chunks currently visible according to RenderDistance. */
	int32 GetVisibleChunkCount() const { return VisibleCoordinates.Num(); }

	/** Number of chunk requests queued waiting for frame budget. */
	int32 GetPendingRequestCount() const { return FMath::Max(0, PendingRequests.Num() - PendingRequestIndex); }

	/** Number of chunk unloads queued waiting for frame budget. */
	int32 GetPendingUnloadCount() const { return FMath::Max(0, PendingUnloads.Num() - PendingUnloadIndex); }

	/** Actual time spent executing streaming work during the last Tick, in milliseconds. */
	float GetLastTickBudgetUsedMs() const { return LastTickBudgetUsedMs; }

	/** Freezes dynamic chunk requesting/unloading for steady-state diagnostic benchmarking (Mode D). */
	void SetStreamingFrozen(bool bFrozen)
	{
		bStreamingFrozen = bFrozen;
		if (!bStreamingFrozen)
		{
			bForceQueueReevaluation = true;
		}
	}
	bool IsStreamingFrozen() const { return bStreamingFrozen; }

	/** Forces immediate re-evaluation of desired streaming coordinates on next tick. */
	void ForceReevaluateQueue() { bForceQueueReevaluation = true; }

	/** Clears all managed coordinates (used for baseline Mode A). */
	void ClearAllManaged();

	// Runtime Distance Band Controls (Stage D)
	void SetRenderDistance(int32 InRenderDistance);
	int32 GetRenderDistance() const { return RenderDistance; }

	void SetSimulationDistance(int32 InSimulationDistance);
	int32 GetSimulationDistance() const { return SimulationDistance; }

	void SetGenerationDistance(int32 InGenerationDistance);
	int32 GetGenerationDistance() const { return GenerationDistance; }

	void SetPersistenceDistance(int32 InPersistenceDistance);
	int32 GetPersistenceDistance() const { return PersistenceDistance; }

	void SetStreamingBudgetMs(float InBudgetMs);
	float GetStreamingBudgetMs() const { return StreamingBudgetMs; }

	/** Maps a chunk coordinate's distance relative to the viewer to an asynchronous scheduler priority. */
	EVoxelWorkPriority GetPriorityForCoordinate(const FVoxelChunkCoordinate& Coordinate, const FVoxelChunkCoordinate& ViewerChunk) const;

	/** Maps a Chebyshev distance to an asynchronous scheduler priority. */
	FORCEINLINE EVoxelWorkPriority GetPriorityForDistance(int32 Dist) const
	{
		if (Dist <= SimulationDistance) return EVoxelWorkPriority::Critical;
		if (Dist <= RenderDistance)     return EVoxelWorkPriority::High;
		if (Dist <= GenerationDistance) return EVoxelWorkPriority::Normal;
		return EVoxelWorkPriority::Low;
	}

private:
	FVoxelChunkCoordinate WorldToChunkCoordinate(const FVector& WorldPosition) const;
	FVector GetAutoViewerPosition() const;
	void RebuildCachedOffsets();

	UVoxelWorldSubsystem* WorldSubsystem = nullptr;
	bool bStreamingFrozen = false;
	bool bForceQueueReevaluation = false;

	// Configuration (read once at Initialize from UVoxelRuntimeSettings)
	int32 ChunkSize = 32;
	int32 WorldHeightInChunks = 8;
	int32 SimulationDistance = 4;
	int32 RenderDistance = 8;
	int32 GenerationDistance = 10;
	int32 PersistenceDistance = 12;
	float StreamingBudgetMs = 1.5f;
	float VoxelWorldSize = 100.0f;
	float ChunkWorldEdgeSize = 3200.0f;
	float InvChunkWorldEdgeSize = 1.0f / 3200.0f;

	// Cached relative offsets sorted by Chebyshev distance (rebuilt only on distance changes)
	TArray<FIntVector> CachedRelativeOffsets;

	// State
	FVoxelChunkCoordinate LastViewerChunk = FVoxelChunkCoordinate(INT32_MAX, INT32_MAX, INT32_MAX);
	TSet<FVoxelChunkCoordinate> ManagedCoordinates; // coordinates this manager has requested
	TSet<FVoxelChunkCoordinate> VisibleCoordinates; // coordinates currently set visible
	TArray<FVoxelChunkCoordinate> PendingRequests;   // sorted by ascending distance, carried across ticks
	int32 PendingRequestIndex = 0;
	TArray<FVoxelChunkCoordinate> PendingUnloads;    // carried across ticks
	int32 PendingUnloadIndex = 0;

	// Manual viewer override
	FVector ManualViewerPosition = FVector(FLT_MAX);
	bool bUseManualViewer = false;

	float LastTickBudgetUsedMs = 0.0f;

	bool bFirstTick = true;
};
