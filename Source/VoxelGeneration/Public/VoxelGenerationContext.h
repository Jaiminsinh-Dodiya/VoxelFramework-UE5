// VoxelGenerationContext.h
//
// Purpose:
//   Everything a single generation pass needs to do its job, bundled into
//   one struct so pass signatures stay uniform (see IVoxelGenerationPass).
//   Per-column intermediate results (height, temperature, humidity, biome)
//   live here too - later passes read what earlier passes computed instead
//   of recomputing it or reaching into chunk storage for derived data.
//
// Responsibilities: plain data + the per-column scratch arrays passes
//   read/write. No logic beyond trivial accessors.
// Thread ownership: one context per in-flight chunk generation job: never
//   shared across chunks, never touched off the worker thread generating
//   that chunk. Safe to pass by reference through the whole pass pipeline.
// Dependencies: Core, VoxelCore, VoxelAssets (block/biome registry access).

#pragma once

#include "CoreMinimal.h"
#include "VoxelCoreTypes.h"

class UVoxelBlockRegistry;
class UVoxelBiomeDefinition;

/** Per-column (X,Y within the chunk) values computed by early passes and consumed by later ones. */
struct FVoxelColumnData
{
	float Temperature = 0.5f;  // [0,1], written by ClimatePass
	float Humidity = 0.5f;     // [0,1], written by ClimatePass
	int32 TerrainHeight = 0;   // world-space height, written by TerrainPass
	const UVoxelBiomeDefinition* Biome = nullptr; // written by BiomePass
};

struct FVoxelGenerationContext
{
	int32 WorldSeed = 0;
	FVoxelChunkCoordinate ChunkCoordinate;
	int32 ChunkSize = 32;

	const UVoxelBlockRegistry* BlockRegistry = nullptr;
	TArray<const UVoxelBiomeDefinition*> AvailableBiomes; // candidate set BiomePass selects from

	// Indexed [X + Y * ChunkSize] - one entry per column in this chunk, shared across all Z passes.
	TArray<FVoxelColumnData> Columns;

	void InitColumns()
	{
		Columns.SetNum(ChunkSize * ChunkSize);
	}

	FVoxelColumnData& ColumnAt(int32 LocalX, int32 LocalY)
	{
		return Columns[LocalX + LocalY * ChunkSize];
	}

	const FVoxelColumnData& ColumnAt(int32 LocalX, int32 LocalY) const
	{
		return Columns[LocalX + LocalY * ChunkSize];
	}

	/** World-space X/Y for a local column coordinate - passes sample noise in world space so chunks tile seamlessly. */
	FVector2D LocalToWorldColumn(int32 LocalX, int32 LocalY) const
	{
		return FVector2D(
			ChunkCoordinate.X * ChunkSize + LocalX,
			ChunkCoordinate.Y * ChunkSize + LocalY);
	}
};
