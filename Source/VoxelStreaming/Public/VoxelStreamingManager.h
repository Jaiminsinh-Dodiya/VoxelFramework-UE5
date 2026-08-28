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

	/** Number of chunk requests queued waiting for frame budget. */
	int32 GetPendingRequestCount() const { return PendingRequests.Num(); }

	/** Number of chunk unloads queued waiting for frame budget. */
	int32 GetPendingUnloadCount() const { return PendingUnloads.Num(); }

	/** Actual time spent executing streaming work during the last Tick, in milliseconds. */
	float GetLastTickBudgetUsedMs() const { return LastTickBudgetUsedMs; }

private:
	FVoxelChunkCoordinate WorldToChunkCoordinate(const FVector& WorldPosition) const;
	FVector GetAutoViewerPosition() const;

	UVoxelWorldSubsystem* WorldSubsystem = nullptr;

	// Configuration (read once at Initialize from UVoxelRuntimeSettings)
	int32 ChunkSize = 32;
	int32 WorldHeightInChunks = 8;
	int32 SimulationDistance = 4;
	int32 RenderDistance = 8;
	int32 GenerationDistance = 10;
	int32 PersistenceDistance = 12;
	float StreamingBudgetMs = 1.5f;
	float VoxelWorldSize = 100.0f;

	// State
	FVoxelChunkCoordinate LastViewerChunk = FVoxelChunkCoordinate(INT32_MAX, INT32_MAX, INT32_MAX);
	TSet<FVoxelChunkCoordinate> ManagedCoordinates; // coordinates this manager has requested
	TArray<FVoxelChunkCoordinate> PendingRequests;   // sorted by ascending distance, carried across ticks
	TArray<FVoxelChunkCoordinate> PendingUnloads;    // carried across ticks

	// Manual viewer override
	FVector ManualViewerPosition = FVector(FLT_MAX);
	bool bUseManualViewer = false;

	float LastTickBudgetUsedMs = 0.0f;

	bool bFirstTick = true;
};
