// IVoxelGenerationPass.h
//
// Purpose:
//   Common interface for every stage of the generation pipeline
//   (Terrain -> Climate -> Biome -> River -> Cave -> Structure -> Vegetation).
//   Plain C++ interface, not a UObject/Blueprint extension point yet - per
//   the agreed roadmap, that upgrade is deliberately deferred until the
//   pipeline shape has proven itself with a working vertical slice.
//
// Responsibilities: define the one method every pass implements. Nothing
//   else - passes are otherwise free to be implemented however they like.
// Thread ownership: Execute() runs entirely on whatever worker thread the
//   owning FVoxelGenerationPipeline was dispatched on. Must not touch
//   UObjects other than read-only access to already-loaded data assets
//   (block/biome definitions), and must never touch the Game Thread.
// Dependencies: Core, VoxelStorage (FVoxelChunk), this module's context type.

#pragma once

#include "CoreMinimal.h"

class FVoxelChunk;
struct FVoxelGenerationContext;

class IVoxelGenerationPass
{
public:
	virtual ~IVoxelGenerationPass() = default;

	/** Human-readable name for logging/profiling (e.g. "TerrainPass"). */
	virtual const TCHAR* GetPassName() const = 0;

	/**
	 * Runs this pass against Chunk, reading/writing Context as needed.
	 * Passes must be deterministic: same WorldSeed + ChunkCoordinate always
	 * produces the same result (ADR-005 depends on this).
	 */
	virtual void Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk) = 0;
};
