// BiomePass.h
//
// Purpose: selects one biome per column from Context.AvailableBiomes based
//   on the Temperature/Humidity written by ClimatePass. Must run after
//   ClimatePass and before TerrainPass if biome-specific terrain layering
//   is desired (see FVoxelGenerationPipeline for the actual run order).
// Thread ownership: worker thread only.
// Dependencies: VoxelAssets (UVoxelBiomeDefinition).

#pragma once

#include "IVoxelGenerationPass.h"

class FBiomePass : public IVoxelGenerationPass
{
public:
	virtual const TCHAR* GetPassName() const override { return TEXT("BiomePass"); }
	virtual void Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk) override;
};
