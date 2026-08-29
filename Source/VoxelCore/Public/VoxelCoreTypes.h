// VoxelCoreTypes.h
//
// Purpose:
//   Common value types shared by every Voxel* module: chunk coordinates,
//   block IDs, and the handle types used to reference chunks without
//   passing raw pointers across module boundaries.
//
// Responsibilities:
//   Plain data only. No logic beyond trivial operators/hashing. This file
//   must stay dependency-free (Core only) since everything includes it.
//
// Thread ownership: N/A (value types, no shared mutable state).
// Dependencies: Core only.
// Performance notes: FChunkCoordinate is used as a TMap key on hot paths
//   (streaming lookups happen every tick), so GetTypeHash must stay cheap.

#pragma once

#include "CoreMinimal.h"
#include "VoxelJobTypes.h"

/** Bit-packed block identifier. 16 bits today, room to grow without breaking save format (see VoxelPersistence). */
using FVoxelBlockId = uint16;

constexpr FVoxelBlockId VoxelBlockId_Air = 0;

/** Authoritative lifecycle state of a chunk in memory / streaming pipeline. */
enum class EVoxelChunkState : uint8
{
	Unloaded,        // Not in memory, no storage allocated
	Queued,          // Storage reserved, generation job queued
	Generating,      // Worker thread running generation passes
	Meshing,         // Worker thread running greedy mesher
	PendingFinalize, // Mesh data ready, waiting in Game Thread finalization queue
	Ready,           // Fully generated, meshed, and resident in chunk store
	Unloading        // Unload requested; waiting for in-flight worker jobs before slot recycling
};

/** Integer chunk-space coordinate (not world-space; multiply by chunk size to get world position). */
struct FVoxelChunkCoordinate
{
	int32 X = 0;
	int32 Y = 0;
	int32 Z = 0;

	FVoxelChunkCoordinate() = default;
	FVoxelChunkCoordinate(int32 InX, int32 InY, int32 InZ) : X(InX), Y(InY), Z(InZ) {}

	bool operator==(const FVoxelChunkCoordinate& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	FVoxelChunkCoordinate operator+(const FVoxelChunkCoordinate& Other) const
	{
		return FVoxelChunkCoordinate(X + Other.X, Y + Other.Y, Z + Other.Z);
	}

	/** Chebyshev distance - matches how streaming radii are specified (a "3 chunk radius" is a cube, not a sphere). */
	int32 ChebyshevDistanceTo(const FVoxelChunkCoordinate& Other) const
	{
		return FMath::Max3(FMath::Abs(X - Other.X), FMath::Abs(Y - Other.Y), FMath::Abs(Z - Other.Z));
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelChunkCoordinate& Coord)
{
	// Simple mix; swap for a proper spatial hash (e.g. Morton) only if profiling shows collisions matter.
	return HashCombineFast(HashCombineFast(GetTypeHash(Coord.X), GetTypeHash(Coord.Y)), GetTypeHash(Coord.Z));
}

/** Opaque, non-owning reference to a chunk. Never a raw pointer across module boundaries - see VoxelStorage. */
struct FVoxelChunkHandle
{
	FVoxelChunkCoordinate Coordinate;
	uint32 Generation = 0; // incremented each time a pooled chunk slot is reused, to detect stale handles

	bool IsValid() const { return Generation != 0; }

	bool operator==(const FVoxelChunkHandle& Other) const
	{
		return Coordinate == Other.Coordinate && Generation == Other.Generation;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelChunkHandle& Handle)
{
	return HashCombineFast(GetTypeHash(Handle.Coordinate), GetTypeHash(Handle.Generation));
}
