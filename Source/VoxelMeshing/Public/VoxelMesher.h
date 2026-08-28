// VoxelMesher.h
//
// Purpose:
//   Converts a finished FVoxelChunk into FVoxelMeshData via hidden-face
//   removal + greedy meshing + baked per-vertex ambient occlusion. This is
//   the entire contract: FVoxelChunk -> FVoxelMeshData. Nothing about
//   biomes, regions, islands, or story content is visible here or ever
//   should be - see Docs/ARCHITECTURE.md's design-checkpoint section for
//   why that separation is being protected deliberately.
//
// Responsibilities:
//   - Hidden face removal: never emit a face between two solid voxels
//   - Greedy meshing: merge adjacent coplanar faces of the same material
//     into single quads, per the standard binary-greedy-meshing algorithm
//   - Bake ambient occlusion into vertex color, computed per-vertex from
//     actual neighboring voxel solidity, independent of merge size
//
// Thread ownership: GenerateMesh is a pure function of its inputs - no
//   UObject writes, no Game Thread requirement, safe to call from any
//   worker thread. Callers wanting async dispatch should use
//   FVoxelScheduler directly (see VoxelMeshingService.h for a thin
//   optional helper) - this class does not spawn threads itself.
//
// Dependencies: VoxelCore, VoxelStorage (FVoxelChunk), VoxelAssets
//   (optional - UVoxelBlockRegistry for material/tint resolution).

#pragma once

#include "CoreMinimal.h"
#include "VoxelMeshData.h"

class FVoxelChunk;
class UVoxelBlockRegistry;

class VOXELMESHING_API FVoxelMesher
{
public:
	/**
	 * Generates mesh data for Chunk. BlockRegistry may be nullptr - in that
	 * case, MaterialId falls back to the raw FVoxelBlockId and no per-block
	 * vertex tint is applied (see FVoxelMeshSection::MaterialId comment).
	 *
	 * Chunk-edge voxels are treated as facing air beyond the chunk boundary
	 * (no cross-chunk neighbor lookup) - this means faces at chunk edges are
	 * always emitted even if an adjacent chunk has a solid voxel there.
	 * Stitching seams across chunk boundaries is explicitly NOT handled here
	 * (see Docs/TODO.md) - it is a VoxelStreaming/world-subsystem concern
	 * once chunks are aware of their neighbors.
	 */
	static FVoxelMeshData GenerateMesh(const FVoxelChunk& Chunk, const UVoxelBlockRegistry* BlockRegistry);
};
