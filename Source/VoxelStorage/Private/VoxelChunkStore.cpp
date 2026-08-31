// VoxelChunkStore.cpp

#include "VoxelChunkStore.h"

FVoxelChunkStore::FVoxelChunkStore(int32 InChunkSize)
	: ChunkSize(InChunkSize)
{
	check(ChunkSize > 0);
}

FVoxelChunkHandle FVoxelChunkStore::CreateOrGetChunk(const FVoxelChunkCoordinate& Coordinate)
{
	if (const int32* ExistingSlotIndex = CoordinateToSlot.Find(Coordinate))
	{
		const FSlot& ExistingSlot = Slots[*ExistingSlotIndex];
		return FVoxelChunkHandle{ Coordinate, ExistingSlot.Generation };
	}

	int32 SlotIndex;
	if (FreeSlotIndices.Num() > 0)
	{
		SlotIndex = FreeSlotIndices.Pop(EAllowShrinking::No);
	}
	else
	{
		SlotIndex = Slots.Add(FSlot{});
	}

	FSlot& Slot = Slots[SlotIndex];
	if (!Slot.Chunk.IsValid())
	{
		Slot.Chunk = MakeUnique<FVoxelChunk>(ChunkSize);
	}
	else
	{
		Slot.Chunk->ResetForReuse();
	}

	Slot.Coordinate = Coordinate;
	Slot.Generation += 1; // 0 is reserved as "invalid handle" (see FVoxelChunkHandle::IsValid), so first use becomes generation 1
	Slot.bInUse = true;

	CoordinateToSlot.Add(Coordinate, SlotIndex);

	return FVoxelChunkHandle{ Coordinate, Slot.Generation };
}

void FVoxelChunkStore::RemoveChunk(const FVoxelChunkCoordinate& Coordinate)
{
	const int32* SlotIndexPtr = CoordinateToSlot.Find(Coordinate);
	if (!SlotIndexPtr)
	{
		return;
	}

	const int32 SlotIndex = *SlotIndexPtr;
	FSlot& Slot = Slots[SlotIndex];
	Slot.bInUse = false;
	CoordinateToSlot.Remove(Coordinate);

	// Invariant: only recycle to FreeSlotIndices if no active workers hold a lease on this slot.
	// If workers are active, ReleaseWorkerLease will recycle the slot once the last worker finishes.
	if (Slot.InFlightWorkers == 0)
	{
		FreeSlotIndices.Add(SlotIndex);
	}
}

int32 FVoxelChunkStore::AcquireWorkerLease(const FVoxelChunkCoordinate& Coordinate)
{
	if (const int32* SlotIndexPtr = CoordinateToSlot.Find(Coordinate))
	{
		Slots[*SlotIndexPtr].InFlightWorkers++;
		return *SlotIndexPtr;
	}
	return INDEX_NONE;
}

void FVoxelChunkStore::ReleaseWorkerLease(int32 SlotIndex)
{
	if (SlotIndex >= 0 && SlotIndex < Slots.Num())
	{
		FSlot& Slot = Slots[SlotIndex];
		Slot.InFlightWorkers--;
		check(Slot.InFlightWorkers >= 0);

		// If the chunk was unloaded while the worker was active, recycle the slot now that all workers are done.
		if (!Slot.bInUse && Slot.InFlightWorkers == 0)
		{
			FreeSlotIndices.Add(SlotIndex);
		}
	}
}

bool FVoxelChunkStore::IsSlotBusy(int32 SlotIndex) const
{
	return (SlotIndex >= 0 && SlotIndex < Slots.Num()) ? (Slots[SlotIndex].InFlightWorkers > 0) : false;
}

FVoxelChunk* FVoxelChunkStore::FindChunkByCoordinate(const FVoxelChunkCoordinate& Coordinate) const
{
	const int32* SlotIndexPtr = CoordinateToSlot.Find(Coordinate);
	if (!SlotIndexPtr)
	{
		return nullptr;
	}

	const FSlot& Slot = Slots[*SlotIndexPtr];
	return Slot.bInUse ? Slot.Chunk.Get() : nullptr;
}

FVoxelChunk* FVoxelChunkStore::FindChunkByHandle(const FVoxelChunkHandle& Handle) const
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	const int32* SlotIndexPtr = CoordinateToSlot.Find(Handle.Coordinate);
	if (!SlotIndexPtr)
	{
		return nullptr;
	}

	const FSlot& Slot = Slots[*SlotIndexPtr];
	if (!Slot.bInUse || Slot.Generation != Handle.Generation)
	{
		return nullptr; // stale handle - slot was reused for a different load since this handle was captured
	}

	return Slot.Chunk.Get();
}
