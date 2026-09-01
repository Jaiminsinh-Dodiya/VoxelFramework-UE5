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
class UVoxelStreamingPreset;

UCLASS()
class VOXELSTREAMING_API UVoxelStreamingManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Applies streaming distance bands and budget parameters from a UVoxelStreamingPreset asset. */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (ToolTip = "Applies streaming distance bands and budget parameters from a preset asset."))
	void ApplyPreset(const UVoxelStreamingPreset* Preset);

	/** Override the auto-tracked viewer position. Pass FLT_MAX to re-enable auto-tracking. */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (ToolTip = "Overrides the auto-tracked viewer position. The input is a world-space location (e.g., from GetActorLocation)."))
	void SetViewerPosition(const FVector& WorldPosition);

	/** Number of chunks currently managed by this streaming manager. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Returns the number of chunks currently managed by this streaming manager."))
	int32 GetManagedChunkCount() const { return ManagedCoordinates.Num(); }

	/** Number of chunks currently visible according to RenderDistance. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Returns the number of chunks currently visible according to the Render Distance."))
	int32 GetVisibleChunkCount() const { return VisibleCoordinates.Num(); }

	/** Number of chunk requests queued waiting for frame budget. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Returns the number of chunk loading requests queued up and waiting for frame budget."))
	int32 GetPendingRequestCount() const { return FMath::Max(0, PendingRequests.Num() - PendingRequestIndex); }

	/** Number of chunk unloads queued waiting for frame budget. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Returns the number of chunk unloads queued up and waiting for frame budget."))
	int32 GetPendingUnloadCount() const { return FMath::Max(0, PendingUnloads.Num() - PendingUnloadIndex); }

	/** Actual time spent executing streaming work during the last Tick, in milliseconds. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Returns the actual time spent executing streaming work during the last Tick, in milliseconds."))
	float GetLastTickBudgetUsedMs() const { return LastTickBudgetUsedMs; }

	/** Freezes dynamic chunk requesting/unloading for steady-state diagnostic benchmarking (Mode D). */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Development", meta = (ToolTip = "Freezes dynamic chunk requesting and unloading for steady-state diagnostic benchmarking."))
	void SetStreamingFrozen(bool bFrozen)
	{
		bStreamingFrozen = bFrozen;
		if (!bStreamingFrozen)
		{
			bForceQueueReevaluation = true;
		}
	}

	UFUNCTION(BlueprintPure, Category = "Voxel|Development", meta = (ToolTip = "Returns true if the streaming manager is currently frozen and not loading or unloading chunks."))
	bool IsStreamingFrozen() const { return bStreamingFrozen; }

	/** Forces immediate re-evaluation of desired streaming coordinates on next tick. */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Development", meta = (ToolTip = "Forces immediate re-evaluation of desired streaming coordinates on the next tick."))
	void ForceReevaluateQueue() { bForceQueueReevaluation = true; }

	/** Clears all managed coordinates (used for baseline Mode A). */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Development", meta = (ToolTip = "Clears all currently managed coordinates, dropping all chunks instantly. Used for testing."))
	void ClearAllManaged();

	// Runtime Distance Band Controls (Stage D)
	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (ToolTip = "Sets the maximum visible distance in chunk units (not world units)."))
	void SetRenderDistance(int32 InRenderDistance);

	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Gets the current render distance measured in chunk units."))
	int32 GetRenderDistance() const { return RenderDistance; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (ToolTip = "Sets the distance in chunk units (not world units) where gameplay simulation occurs."))
	void SetSimulationDistance(int32 InSimulationDistance);

	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Gets the current simulation distance measured in chunk units."))
	int32 GetSimulationDistance() const { return SimulationDistance; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (ToolTip = "Sets the distance in chunk units (not world units) where new chunks are generated."))
	void SetGenerationDistance(int32 InGenerationDistance);

	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Gets the current generation distance measured in chunk units."))
	int32 GetGenerationDistance() const { return GenerationDistance; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (ToolTip = "Sets the distance in chunk units (not world units) beyond which chunks are unloaded and destroyed."))
	void SetPersistenceDistance(int32 InPersistenceDistance);

	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Gets the current persistence distance measured in chunk units."))
	int32 GetPersistenceDistance() const { return PersistenceDistance; }

	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (ToolTip = "Sets the maximum time in milliseconds the streaming manager can spend processing per frame."))
	void SetStreamingBudgetMs(float InBudgetMs);

	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (ToolTip = "Gets the current time budget in milliseconds allocated for streaming work per frame."))
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
