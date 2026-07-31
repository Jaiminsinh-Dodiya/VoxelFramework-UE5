// VoxelGenerationPipeline.h
//
// Purpose:
//   Owns the ordered list of passes and runs them against a chunk. This is
//   the single place the pass order (Terrain -> Climate -> Biome -> River
//   -> Cave -> Structure -> Vegetation) is defined - individual passes
//   don't know what runs before/after them, they only know what they read
//   from FVoxelGenerationContext.
//
// Responsibilities: own passes, run them in order, initialize per-chunk
//   context (column array) before the first pass runs.
// Thread ownership: GenerateChunk() runs entirely on a worker thread (per
//   IVoxelGenerationPass contract) - the pipeline itself has no Game
//   Thread-only state.
// Dependencies: Core, VoxelStorage, VoxelAssets, this module's own types.

#pragma once

#include "CoreMinimal.h"
#include "VoxelGenerationContext.h"

class IVoxelGenerationPass;
class FVoxelChunk;
class UVoxelBlockRegistry;
class UVoxelBiomeDefinition;

class VOXELGENERATION_API FVoxelGenerationPipeline
{
public:
	/** Builds the default pass order. Call once at world init. */
	FVoxelGenerationPipeline();
	~FVoxelGenerationPipeline();

	FVoxelGenerationPipeline(const FVoxelGenerationPipeline&) = delete;
	FVoxelGenerationPipeline& operator=(const FVoxelGenerationPipeline&) = delete;

	/**
	 * Generates Chunk's contents deterministically from WorldSeed + its
	 * coordinate. All writes go through FVoxelChunk::SetBlock with
	 * bIsGenerationWrite=true (see ADR-005 - generation output is never
	 * itself part of the save diff).
	 */
	void GenerateChunk(
		int32 WorldSeed,
		const FVoxelChunkCoordinate& Coordinate,
		int32 ChunkSize,
		const UVoxelBlockRegistry* BlockRegistry,
		const TArray<const UVoxelBiomeDefinition*>& AvailableBiomes,
		FVoxelChunk& OutChunk) const;

private:
	TArray<TUniquePtr<IVoxelGenerationPass>> Passes;
};
