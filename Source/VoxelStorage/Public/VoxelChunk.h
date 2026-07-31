// VoxelChunk.h
//
// Purpose:
//   The actual voxel data container. Plain C++ type (ADR-003), never a
//   UObject, never an AActor. Owns one dense array of block IDs plus a
//   sparse modification record used for diff-based serialization (ADR-005).
//
// Responsibilities:
//   - Store/retrieve block IDs by local (X,Y,Z) within the chunk
//   - Track which voxels differ from their originally-generated value
//   - Nothing about meshing, rendering, or generation - a chunk answers
//     "what block is here" and "has this been modified," full stop
//
// Thread ownership:
//   A single FVoxelChunk is NOT internally synchronized. The owning
//   subsystem (VoxelStorage's pool / the future VoxelWorldSubsystem) is
//   responsible for ensuring only one thread mutates a given chunk at a
//   time (generation writes it once on a worker thread before it's handed
//   off; after that, gameplay edits happen on the Game Thread which then
//   hands read-only snapshots to meshing workers). This single-writer
//   assumption is what keeps per-voxel access branch-light and lock-free.
//
// Dependencies: Core, VoxelCore only (FVoxelBlockId, FVoxelChunkCoordinate).
//
// Performance notes:
//   Storage is a flat TArray<FVoxelBlockId> indexed via
//   x + y*Size + z*Size*Size - contiguous and cache-friendly for the
//   greedy-meshing sweep patterns VoxelMeshing will use later. Chunk size
//   is fixed per-instance at construction (read from UVoxelRuntimeSettings
//   by the pool, not hardcoded here) so this class stays test-friendly
//   without spinning up the full settings system.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCoreTypes.h"

class VOXELSTORAGE_API FVoxelChunk
{
public:
	explicit FVoxelChunk(int32 InSize);

	/** Resets this chunk instance for reuse from the pool - clears data and modification tracking, keeps allocated capacity. */
	void ResetForReuse();

	int32 GetSize() const { return Size; }

	FVoxelBlockId GetBlock(int32 LocalX, int32 LocalY, int32 LocalZ) const;

	/**
	 * Sets a block. IsGenerationWrite=true means "this is the deterministic
	 * generation pass writing its baseline value" and does NOT mark the
	 * voxel dirty; false means "this is a gameplay edit" and DOES mark it
	 * dirty for diff serialization. Generation must finish before any
	 * gameplay edit occurs for a given chunk (enforced by the streaming
	 * pipeline, not by this class).
	 */
	void SetBlock(int32 LocalX, int32 LocalY, int32 LocalZ, FVoxelBlockId BlockId, bool bIsGenerationWrite);

	bool IsEmpty() const { return bIsEmpty; }

	/** Modified-voxel diff for VoxelSerialization: local linear index -> current block ID. Empty if nothing was ever gameplay-edited. */
	const TMap<int32, FVoxelBlockId>& GetModifications() const { return Modifications; }

	/** Applies a previously-saved diff on load. Does not itself run generation - caller must generate baseline first. */
	void ApplyModifications(const TMap<int32, FVoxelBlockId>& InModifications);

	FORCEINLINE int32 ToLinearIndex(int32 LocalX, int32 LocalY, int32 LocalZ) const
	{
		return LocalX + LocalY * Size + LocalZ * Size * Size;
	}

private:
	int32 Size = 0;
	TArray<FVoxelBlockId> Blocks; // length Size^3, index via ToLinearIndex
	TMap<int32, FVoxelBlockId> Modifications; // linear index -> current value, only for gameplay-edited voxels
	bool bIsEmpty = true; // true until any non-air block is written; lets streaming skip meshing all-air chunks
};
