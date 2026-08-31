// VoxelCollisionData.h
//
// Purpose:
//   Plain CPU-side snapshot container for chunk collision geometry.
//   Generated on worker threads, consumed by UVoxelCollisionComponent / UBodySetup.
//
// Invariant:
//   Contains NO UObject pointers, NO Chaos resources, and NO references to
//   mutable chunk storage. Completely safe to move across threads.
//
// Thread ownership: Value type (movable).
// Dependencies: Core, Engine (FTriIndices), VoxelCore (FVoxelChunkCoordinate).

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "VoxelCoreTypes.h"

struct VOXELPHYSICS_API FVoxelCollisionData
{
	/** World-space or chunk-local vertex positions (single-precision FVector3f for memory efficiency). */
	TArray<FVector3f> Vertices;

	/** Triangle index triplets referencing Vertices. Length is always 3 * TriangleCount. */
	TArray<FTriIndices> Indices;

	/** Spatial bounding box of collision geometry. */
	FBox Bounds = FBox(ForceInit);

	/** Coordinate of the chunk this collision data was generated for. */
	FVoxelChunkCoordinate Coordinate;

	/** Monotonically increasing revision number to detect and discard stale async completions. */
	uint32 CollisionRevision = 0;

	/** True if chunk contains only air or non-collidable voxels. */
	bool bIsEmpty = true;

	FVoxelCollisionData() = default;

	bool IsEmpty() const
	{
		return bIsEmpty || Indices.IsEmpty() || Vertices.IsEmpty();
	}

	int32 GetTriangleCount() const
	{
		return Indices.Num();
	}

	int32 GetVertexCount() const
	{
		return Vertices.Num();
	}

	void Reset()
	{
		Vertices.Reset();
		Indices.Reset();
		Bounds.Init();
		Coordinate = FVoxelChunkCoordinate();
		CollisionRevision = 0;
		bIsEmpty = true;
	}
};
