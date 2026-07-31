// VoxelChunk.cpp

#include "VoxelChunk.h"

FVoxelChunk::FVoxelChunk(int32 InSize)
	: Size(InSize)
{
	check(Size > 0);
	Blocks.SetNumZeroed(Size * Size * Size); // zero = VoxelBlockId_Air
}

void FVoxelChunk::ResetForReuse()
{
	// Keep the allocation (SetNumZeroed on an already-sized array doesn't
	// reallocate), just clear contents - this is the whole point of pooling.
	FMemory::Memzero(Blocks.GetData(), Blocks.Num() * sizeof(FVoxelBlockId));
	Modifications.Reset();
	bIsEmpty = true;
}

FVoxelBlockId FVoxelChunk::GetBlock(int32 LocalX, int32 LocalY, int32 LocalZ) const
{
	const int32 Index = ToLinearIndex(LocalX, LocalY, LocalZ);
	if (!Blocks.IsValidIndex(Index))
	{
		return VoxelBlockId_Air;
	}
	return Blocks[Index];
}

void FVoxelChunk::SetBlock(int32 LocalX, int32 LocalY, int32 LocalZ, FVoxelBlockId BlockId, bool bIsGenerationWrite)
{
	const int32 Index = ToLinearIndex(LocalX, LocalY, LocalZ);
	if (!Blocks.IsValidIndex(Index))
	{
		return;
	}

	Blocks[Index] = BlockId;

	if (BlockId != VoxelBlockId_Air)
	{
		bIsEmpty = false;
	}

	if (!bIsGenerationWrite)
	{
		Modifications.Add(Index, BlockId);
	}
}

void FVoxelChunk::ApplyModifications(const TMap<int32, FVoxelBlockId>& InModifications)
{
	for (const TPair<int32, FVoxelBlockId>& Pair : InModifications)
	{
		if (Blocks.IsValidIndex(Pair.Key))
		{
			Blocks[Pair.Key] = Pair.Value;
			if (Pair.Value != VoxelBlockId_Air)
			{
				bIsEmpty = false;
			}
		}
	}
	Modifications = InModifications;
}
