// VoxelWorldSubsystem.h
//
// Purpose:
//   The integration point: given a chunk coordinate, produces a rendered
//   chunk asynchronously, hiding generation + meshing + component/material
//   setup behind one call. This is the piece VoxelDebug fakes by hand
//   (calling PrecacheBiomeLayers per-call, running generation/meshing
//   synchronously on the Game Thread, manually managing preview
//   components) - this class does it once, correctly, for real use.
//
// Explicit scope boundary (see TODO.md / Docs/ARCHITECTURE.md):
//   - This class does NOT decide WHEN or WHY to request a chunk (no
//     distance-to-player logic, no automatic loading) - that is
//     VoxelStreaming's job. RequestChunk is called externally by
//     UVoxelStreamingManager or diagnostic visualizers.
//   - UnloadChunk cancels in-flight jobs via FVoxelScheduler::RequestCancel.
//     Note that RequestCancel only prevents a Queued job from starting;
//     a Running job's Work() completes normally (state stays Cancelled).
//     FinalizeChunkMesh guards against this by checking RequestedCoordinates
//     before acting on the result — this is the actual fix, not optional
//     defense-in-depth (cancelled-while-running is the common case since
//     generation+meshing typically finishes near-instantly).
//
// Responsibilities:
//   - Own the FVoxelChunkStore for this world
//   - Call UVoxelBlockRegistry::PrecacheBiomeLayers ONCE at Initialize,
//     not per chunk request
//   - RequestChunk: reserve a chunk slot synchronously, dispatch
//     generation+meshing as one worker-thread job via FVoxelScheduler,
//     marshal the result back to the Game Thread to create/update a
//     UVoxelMeshComponent
//   - UnloadChunk: remove the chunk's storage and rendering
//
// Thread ownership: all PUBLIC methods are Game-Thread-only (matches
//   FVoxelChunkStore's own current Game-Thread-only contract - see its
//   header). The dispatched generation+meshing work runs on a worker
//   thread; only the final component update happens back on the Game
//   Thread, via an explicit marshal (FVoxelScheduler's OnComplete does NOT
//   guarantee Game Thread - see VoxelScheduler.h).
//
// Dependencies: VoxelRuntime (FVoxelScheduler), VoxelAssets
//   (UVoxelBlockRegistry), VoxelStorage (FVoxelChunkStore),
//   VoxelGeneration (FVoxelGenerationPipeline), VoxelMeshing
//   (FVoxelMesher), VoxelRendering (UVoxelMeshComponent).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VoxelCoreTypes.h"
#include "VoxelChunkStore.h"
#include "VoxelJobTypes.h"
#include "Tickable.h"
#include "VoxelMeshData.h"
#include "VoxelCollisionData.h"
#include "VoxelPhysicsTypes.h"
#include "VoxelGenerationConfig.h"
#include "VoxelWorldSubsystem.generated.h"

class FVoxelChunk;
class UVoxelBlockRegistry;
class UVoxelBiomeDefinition;
class UVoxelWorldDefinition;
class UVoxelMeshComponent;
class UVoxelCollisionComponent;
class UMaterialInterface;
class AVoxelWorldRenderActor;


struct FVoxelCompletedMeshItem
{
	FVoxelChunkCoordinate Coordinate;
	int32 SlotIndex = INDEX_NONE;
	TArray<int32> NeighborSlotIndices;
	FVoxelMeshData MeshData;
	double QueueEntryTime = 0.0;
	bool bIsRemesh = false;

	FVoxelCompletedMeshItem() = default;
	FVoxelCompletedMeshItem(
		const FVoxelChunkCoordinate& InCoordinate,
		int32 InSlotIndex,
		TArray<int32>&& InNeighborSlots,
		FVoxelMeshData&& InMeshData,
		double InQueueEntryTime,
		bool bInIsRemesh = false)
		: Coordinate(InCoordinate)
		, SlotIndex(InSlotIndex)
		, NeighborSlotIndices(MoveTemp(InNeighborSlots))
		, MeshData(MoveTemp(InMeshData))
		, QueueEntryTime(InQueueEntryTime)
		, bIsRemesh(bInIsRemesh)
	{
	}
};

struct FVoxelCompletedCollisionItem
{
	FVoxelChunkCoordinate Coordinate;
	int32 SlotIndex = INDEX_NONE;
	TArray<int32> NeighborSlotIndices;
	FVoxelCollisionData CollisionData;
	double QueueEntryTime = 0.0;
	uint32 CollisionRevision = 0;

	FVoxelCompletedCollisionItem() = default;
	FVoxelCompletedCollisionItem(
		const FVoxelChunkCoordinate& InCoordinate,
		int32 InSlotIndex,
		TArray<int32>&& InNeighborSlots,
		FVoxelCollisionData&& InCollisionData,
		double InQueueEntryTime,
		uint32 InRevision)
		: Coordinate(InCoordinate)
		, SlotIndex(InSlotIndex)
		, NeighborSlotIndices(MoveTemp(InNeighborSlots))
		, CollisionData(MoveTemp(InCollisionData))
		, QueueEntryTime(InQueueEntryTime)
		, CollisionRevision(InRevision)
	{
	}
};

UCLASS()
class VOXELWORLD_API UVoxelWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual ~UVoxelWorldSubsystem() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * Reserves storage for Coordinate (synchronous, cheap) and dispatches
	 * generation+meshing asynchronously. Idempotent - calling again for an
	 * already-requested-or-loaded coordinate just returns the existing
	 * handle without dispatching a second job.
	 */
	FVoxelChunkHandle RequestChunk(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority = EVoxelWorkPriority::Normal);

	/** Removes the chunk's storage and rendering. Cancels any in-flight generation/meshing job for this coordinate via FVoxelScheduler::RequestCancel. */
	void UnloadChunk(const FVoxelChunkCoordinate& Coordinate, bool bTriggerNeighborRemesh = true);

	/** Read-only access to already-generated chunk data. Returns nullptr if not requested, still generating, or unloaded. */
	const FVoxelChunk* FindChunk(const FVoxelChunkCoordinate& Coordinate) const;

	/** True once the chunk has been generated and meshed into the ready state. */
	bool IsChunkReady(const FVoxelChunkCoordinate& Coordinate) const;

	/** Authoritative lifecycle state of a chunk coordinate. */
	EVoxelChunkState GetChunkState(const FVoxelChunkCoordinate& Coordinate) const;

	/** Requests an asynchronous remesh for an already resident ready chunk (e.g. when neighboring chunk arrives or unloads). */
	void RequestRemeshChunk(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority = EVoxelWorkPriority::Normal);

	// VoxelPhysics Collision Management APIs
	/** Requests physical collision generation and Chaos cooking for an existing resident chunk. */
	void RequestChunkCollision(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority = EVoxelWorkPriority::High);

	/** Unloads and destroys collision representation for a chunk. */
	void UnloadChunkCollision(const FVoxelChunkCoordinate& Coordinate);

	/** Returns current collision lifecycle state for a chunk coordinate. */
	EVoxelCollisionState GetChunkCollisionState(const FVoxelChunkCoordinate& Coordinate) const;

	/** Returns true if chunk currently has registered active physical collision. */
	bool HasChunkCollision(const FVoxelChunkCoordinate& Coordinate) const;

	/** Toggle rendering visibility for a chunk's mesh component. No-op if chunk has no component (in-flight or all-air). */
	void SetChunkVisible(const FVoxelChunkCoordinate& Coordinate, bool bVisible);

	/** Sets dynamic shadow casting across all voxel mesh components (useful for isolating VSM impact). */
	void SetCastShadows(bool bInCastShadows);
	bool GetCastShadows() const { return bCastShadows; }

	/** If enabled, worker pipeline generates and meshes chunks on CPU but bypasses GameThread render component creation (Mode C isolation). */
	void SetCpuOnlyMode(bool bInCpuOnly) { bCpuOnlyMode = bInCpuOnly; }
	bool IsCpuOnlyMode() const { return bCpuOnlyMode; }

	/** Unloads all active chunks and cancels in-flight jobs. */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	void ClearAllChunks();

	/** Applies world definition asset (seed, scale, biomes, materials, generation, presets). */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World")
	void ApplyWorldDefinition(const UVoxelWorldDefinition* InWorldDefinition);

	UFUNCTION(BlueprintPure, Category = "Voxel|World")
	int32 GetChunkSize() const { return ChunkSize; }

	UFUNCTION(BlueprintPure, Category = "Voxel|World")
	int32 GetWorldSeed() const { return WorldSeed; }

	UFUNCTION(BlueprintPure, Category = "Voxel|World")
	float GetVoxelWorldSize() const { return VoxelWorldSize; }

	UFUNCTION(BlueprintPure, Category = "Voxel|World")
	bool IsWorldInitialized() const { return ChunkStore.IsValid(); }

	/**
	 * Queries the block ID at a given world position.
	 * Returns true if the chunk containing this position is resident, false otherwise.
	 * Will NOT trigger chunk generation or loading.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Query")
	bool TryGetBlockAtWorldPosition(const FVector& WorldPosition, int32& OutBlockId) const;

	/**
	 * Queries whether the block at a given world position is solid.
	 * Returns true if the chunk containing this position is resident, false otherwise.
	 * Will NOT trigger chunk generation or loading.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Query")
	bool TryIsSolidAtWorldPosition(const FVector& WorldPosition, bool& bOutIsSolid) const;

	/** Converts world coordinates to integer chunk coordinates. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Query")
	FIntVector WorldPositionToChunkCoordinate(const FVector& WorldPosition) const;

	/** Returns true if the specified chunk coordinate has completed generation and is resident. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Chunk")
	bool IsChunkLoaded(const FIntVector& ChunkCoord) const;

	/** Returns true if the specified chunk coordinate has active collision ready. */
	UFUNCTION(BlueprintPure, Category = "Voxel|Chunk")
	bool IsChunkCollisionReady(const FIntVector& ChunkCoord) const;

	UFUNCTION(BlueprintPure, Category = "Voxel|Chunk")
	int32 GetReadyChunkCount() const { return ReadyCoordinates.Num(); }

	UFUNCTION(BlueprintPure, Category = "Voxel|Chunk")
	int32 GetRequestedChunkCount() const { return RequestedCoordinates.Num(); }

	int32 GetFinalizationQueueDepth() const { return FinalizationQueueDepth; }
	float GetLastFinalizeBudgetUsedMs() const { return LastFinalizeBudgetUsedMs; }
	int32 GetLastFinalizeCount() const { return LastFinalizeCount; }

	// Component Pool Metrics
	int32 GetActiveComponentCount() const { return ChunkMeshComponents.Num(); }
	int32 GetActiveCollisionComponentCount() const { return ChunkCollisionComponents.Num(); }
	int32 GetPooledComponentCount() const { return ComponentPool.Num(); }
	int32 GetCreatedComponentCount() const { return CreatedComponentCount; }
	int32 GetReusedComponentCount() const { return ReusedComponentCount; }
	int32 GetDestroyedComponentCount() const { return DestroyedComponentCount; }
	int32 GetPeakPoolSize() const { return PeakPoolSize; }

	// Finalization Queue Latency Metrics
	float GetAverageQueueLatencyMs() const { return AverageQueueLatencyMs; }
	float GetMaxQueueLatencyMs() const { return MaxQueueLatencyMs; }
	float GetOldestQueueItemAgeMs() const { return OldestQueueItemAgeMs; }
	float CalculateQueueLatencyPercentile(float Percentile) const;
	void ResetLatencyStats();

	void SetMaxComponentPoolSize(int32 InMaxSize) { MaxComponentPoolSize = FMath::Max(0, InMaxSize); }
	int32 GetMaxComponentPoolSize() const { return MaxComponentPoolSize; }


private:
	void ProcessCompletedMeshQueue(float DeltaTime);
	void FinalizeChunkMesh(FVoxelCompletedMeshItem&& Item);
	UVoxelMeshComponent* GetOrCreateMeshComponent(const FVoxelChunkCoordinate& Coordinate);
	UMaterialInterface* ResolveMaterialForId(int32 MaterialId) const;

	bool bCastShadows = true;
	bool bCpuOnlyMode = false;

	TUniquePtr<FVoxelChunkStore> ChunkStore;
	TSharedPtr<class FVoxelGenerationPipeline> GenerationPipeline;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelBlockRegistry> BlockRegistry;

	// Strong refs so resolved biomes stay loaded for the subsystem's lifetime.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelBiomeDefinition>> ResolvedBiomes;
	TArray<const UVoxelBiomeDefinition*> AvailableBiomes; // raw-pointer view of ResolvedBiomes, for passing into generation calls

	UPROPERTY(Transient)
	TObjectPtr<AVoxelWorldRenderActor> RenderHostActor;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UMaterialInterface>> ResolvedBlockMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedDefaultMaterial;

	// Component pool strongly owned and GC-rooted by UPROPERTY.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelMeshComponent>> ComponentPool;
	int32 MaxComponentPoolSize = 128;

	// Component Pool telemetry
	int32 CreatedComponentCount = 0;
	int32 ReusedComponentCount = 0;
	int32 DestroyedComponentCount = 0;
	int32 PeakPoolSize = 0;

	// Queue Latency telemetry (samples in ms, rolling window)
	static constexpr int32 MaxQueueLatencySamples = 200;
	TArray<float> QueueLatencyHistory;
	float AverageQueueLatencyMs = 0.0f;
	float MaxQueueLatencyMs = 0.0f;
	float OldestQueueItemAgeMs = 0.0f;
	double TotalQueueLatencyAccumMs = 0.0;
	int64 TotalFinalizedItemsSampled = 0;

	void ProcessCompletedCollisionQueue(float DeltaTime);
	void FinalizeChunkCollision(FVoxelCompletedCollisionItem&& Item);
	void HandleCollisionCookFinished(UVoxelCollisionComponent* Comp, bool bSuccess, uint32 Revision, FVoxelChunkCoordinate Coordinate);
	UVoxelCollisionComponent* GetOrCreateCollisionComponent(const FVoxelChunkCoordinate& Coordinate);

	// Not a UPROPERTY: FVoxelChunkCoordinate is a plain struct (not USTRUCT),
	// so UHT cannot parse it as a TMap key. GC safety is fine because each
	// component's Outer is RenderHostActor, which keeps it rooted.
	TMap<FVoxelChunkCoordinate, TWeakObjectPtr<UVoxelMeshComponent>> ChunkMeshComponents;
	TMap<FVoxelChunkCoordinate, TWeakObjectPtr<UVoxelCollisionComponent>> ChunkCollisionComponents;

	TMap<FVoxelChunkCoordinate, EVoxelChunkState> ChunkStates;
	TMap<FVoxelChunkCoordinate, EVoxelCollisionState> CollisionStates;
	TMap<FVoxelChunkCoordinate, uint32> ChunkCollisionRevisions;

	TSet<FVoxelChunkCoordinate> RequestedCoordinates; // both in-flight and completed - prevents double-dispatch
	TSet<FVoxelChunkCoordinate> ReadyCoordinates;     // generation completed (mesh may still be empty for all-air chunks)
	TSet<FVoxelChunkCoordinate> PendingRemeshCoordinates; // deduplicates in-flight remesh requests

	TMap<FVoxelChunkCoordinate, FVoxelJobHandle> InFlightJobHandles; // coord -> scheduler job
	TMap<FVoxelChunkCoordinate, TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe>> InFlightCancelFlags;

	TMap<FVoxelChunkCoordinate, FVoxelJobHandle> InFlightCollisionJobHandles;
	TMap<FVoxelChunkCoordinate, TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe>> InFlightCollisionCancelFlags;

	TQueue<FVoxelCompletedMeshItem, EQueueMode::Mpsc> CompletedMeshQueue;
	TAtomic<int32> FinalizationQueueDepth{ 0 };

	TQueue<FVoxelCompletedCollisionItem, EQueueMode::Mpsc> CompletedCollisionQueue;
	TAtomic<int32> FinalizationCollisionQueueDepth{ 0 };

	int32 ChunkSize = 32;
	int32 WorldSeed = 1234;
	float VoxelWorldSize = 100.0f;
	float RenderSubmissionBudgetMs = 1.0f;
	FVoxelGenerationConfig GenerationConfig;

	float LastFinalizeBudgetUsedMs = 0.0f;
	int32 LastFinalizeCount = 0;
};

