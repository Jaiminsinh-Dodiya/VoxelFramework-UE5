// VoxelCollisionBuilder.h
//
// Purpose:
//   Pure CPU worker-side builder that converts an FVoxelChunk into an
//   FVoxelCollisionData snapshot.
//
// Invariants:
//   - Pure function of its inputs: no UObject mutation, no Game Thread requirement.
//   - Neighbor-aware: checks leased Ready neighbors to cull internal boundary quads.
//   - Respects UVoxelBlockDefinition::bGeneratesCollision.
//   - Fast-path for empty / all-air chunks.
//
// Thread ownership: Safe from any thread (UE::Tasks worker safe).
// Dependencies: VoxelCore, VoxelStorage, VoxelAssets, VoxelPhysicsTypes, VoxelCollisionData.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCollisionData.h"
#include "VoxelCoreTypes.h"
#include "VoxelPhysicsTypes.h"

class FVoxelChunk;
class UVoxelBlockRegistry;
struct FVoxelNeighborChunks;

class VOXELPHYSICS_API FVoxelCollisionBuilder
{
public:
	/**
	 * Generates a CPU collision geometry snapshot for Chunk.
	 *
	 * @param Chunk              The chunk data to build collision for.
	 * @param BlockRegistry      Registry to query bGeneratesCollision per block definition.
	 * @param Neighbors          Optional leased ready cardinal neighbor chunks.
	 * @param Coordinate         Optional chunk coordinate for worker-side world position transformation.
	 * @param VoxelWorldSize     World-space size of a single voxel in cm (default 100.0f = 1 meter).
	 * @param CollisionRevision  Monotonically increasing revision number for stale rejection.
	 * @param Mode               Collision fidelity mode (default Complex).
	 */
	static FVoxelCollisionData BuildCollisionData(
		const FVoxelChunk& Chunk,
		const UVoxelBlockRegistry* BlockRegistry,
		const FVoxelNeighborChunks* Neighbors = nullptr,
		const FVoxelChunkCoordinate* Coordinate = nullptr,
		float VoxelWorldSize = 100.0f,
		uint32 CollisionRevision = 0,
		EVoxelCollisionMode Mode = EVoxelCollisionMode::Complex);
};
