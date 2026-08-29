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
#include "VoxelCoreTypes.h"

class FVoxelChunk;
class UVoxelBlockRegistry;

/** Read-only pointers to 6 cardinal neighboring chunks for cross-chunk boundary querying and culling. */
struct VOXELMESHING_API FVoxelNeighborChunks
{
	const FVoxelChunk* NegX = nullptr;
	const FVoxelChunk* PosX = nullptr;
	const FVoxelChunk* NegY = nullptr;
	const FVoxelChunk* PosY = nullptr;
	const FVoxelChunk* NegZ = nullptr;
	const FVoxelChunk* PosZ = nullptr;
};

class VOXELMESHING_API FVoxelMesher
{
public:
	/**
	 * Generates mesh data for Chunk via greedy meshing + per-vertex AO.
	 *
	 * When Neighbors is non-null, out-of-chunk voxel lookups read from the adjacent
	 * neighbor chunk, eliminating internal boundary quads. Missing neighbors fall
	 * back safely to air.
	 *
	 * When Coordinate is non-null, vertex positions and bounds are transformed
	 * directly to world space on the worker thread, eliminating Game Thread vertex loops.
	 */
	static FVoxelMeshData GenerateMesh(
		const FVoxelChunk& Chunk,
		const UVoxelBlockRegistry* BlockRegistry,
		const FVoxelNeighborChunks* Neighbors = nullptr,
		const FVoxelChunkCoordinate* Coordinate = nullptr,
		float VoxelWorldSize = 100.0f);
};
