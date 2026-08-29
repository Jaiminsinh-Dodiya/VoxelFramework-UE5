// VoxelChunkStore.h
//
// Purpose:
//   The actual answer to "where do I find chunk X." Owns a pool of
//   FVoxelChunk instances and maps FVoxelChunkCoordinate <-> FVoxelChunkHandle
//   <-> FVoxelChunk*, so every other module references chunks by handle,
//   never by raw pointer or coordinate lookup directly into a map they own
//   themselves.
//
// Responsibilities:
//   - Allocate/reuse FVoxelChunk instances from a pool (avoids per-load
//     heap churn - the "allocation spikes" the original spec rules out)
//   - Generation-counter handles so a handle captured before a chunk was
//     unloaded and its slot reused is detectably stale, not silently wrong
//   - Query/modify/remove by coordinate or handle
//
// Thread ownership:
//   All public methods expected to be called from the Game Thread only in
//   Phase 1 (this is intentionally the simplest possible version - once
//   VoxelStreaming exists in Phase 5, chunk creation will be dispatched
//   through FVoxelScheduler and this class's API will need a thread-safety
//   pass; not done prematurely here per the Phase 1 scope).
//
// Dependencies: Core, VoxelCore, VoxelRuntime (for chunk size lookup via
//   UVoxelRuntimeSettings is intentionally NOT wired here yet - Phase 1
//   takes chunk size as an explicit constructor argument so this class
//   stays unit-testable without spinning up the full settings/subsystem
//   stack. Wiring it to project settings happens when VoxelWorldSubsystem
//   is built in Phase 5).

#pragma once

#include "CoreMinimal.h"
#include "VoxelCoreTypes.h"
#include "VoxelChunk.h"

class VOXELSTORAGE_API FVoxelChunkStore
{
public:
	explicit FVoxelChunkStore(int32 InChunkSize);

	// FSlot owns a TUniquePtr<FVoxelChunk>, making copy inherently unsafe -
	// declare this explicitly rather than relying on the compiler to only
	// implicitly delete it (implicit deletion still lets some contexts try
	// to instantiate operator=, which is what broke the original build).
	FVoxelChunkStore(const FVoxelChunkStore&) = delete;
	FVoxelChunkStore& operator=(const FVoxelChunkStore&) = delete;
	FVoxelChunkStore(FVoxelChunkStore&&) = default;
	FVoxelChunkStore& operator=(FVoxelChunkStore&&) = default;

	/** Creates (or returns the existing) chunk at Coordinate. Returns a valid handle either way. */
	FVoxelChunkHandle CreateOrGetChunk(const FVoxelChunkCoordinate& Coordinate);

	/** Releases the chunk back to the pool. If worker jobs are still active on this slot, recycling to FreeSlotIndices is safely deferred until all workers release their lease. */
	void RemoveChunk(const FVoxelChunkCoordinate& Coordinate);

	/** Returns nullptr if the coordinate has no loaded chunk. */
	FVoxelChunk* FindChunkByCoordinate(const FVoxelChunkCoordinate& Coordinate) const;

	/** Returns nullptr if the handle is stale or unknown - always check before dereferencing. */
	FVoxelChunk* FindChunkByHandle(const FVoxelChunkHandle& Handle) const;

	/** Acquires an asynchronous worker lease on the chunk's storage slot, preventing premature reuse. Returns slot index. */
	int32 AcquireWorkerLease(const FVoxelChunkCoordinate& Coordinate);

	/** Releases an asynchronous worker lease by slot index. If the slot was unloaded, recycles it to FreeSlotIndices once lease count hits 0. */
	void ReleaseWorkerLease(int32 SlotIndex);

	/** Returns true if any asynchronous worker is currently accessing the specified slot index. */
	bool IsSlotBusy(int32 SlotIndex) const;

	int32 GetLoadedChunkCount() const { return CoordinateToSlot.Num(); }
	int32 GetTotalSlotCount() const { return Slots.Num(); }

private:
	struct FSlot
	{
		FSlot() = default;
		FSlot(const FSlot&) = delete;
		FSlot& operator=(const FSlot&) = delete;
		FSlot(FSlot&&) = default;
		FSlot& operator=(FSlot&&) = default;

		TUniquePtr<FVoxelChunk> Chunk;
		FVoxelChunkCoordinate Coordinate;
		uint32 Generation = 0;
		int32 InFlightWorkers = 0;
		bool bInUse = false;
	};

	int32 ChunkSize;
	TArray<FSlot> Slots;
	TArray<int32> FreeSlotIndices; // pool free-list, avoids scanning Slots for a free entry
	TMap<FVoxelChunkCoordinate, int32> CoordinateToSlot;
};
