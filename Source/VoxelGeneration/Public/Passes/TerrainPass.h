// TerrainPass.h
//
// Purpose:
//   First real pass. Computes a per-column height field via fBm noise and
//   fills the chunk's blocks below that height using the selected biome's
//   terrain layers (falls back to a flat stone/dirt/grass default if no
//   biome has been assigned yet - this pass can run before BiomePass in
//   simple projects that don't need biomes).
//
// Responsibilities: write TerrainHeight into each column, write blocks.
// Thread ownership: worker thread only, per IVoxelGenerationPass contract.
// Dependencies: VoxelMath (noise), VoxelAssets (block IDs via biome layers).

#pragma once

#include "IVoxelGenerationPass.h"

class FTerrainPass : public IVoxelGenerationPass
{
public:
	virtual const TCHAR* GetPassName() const override { return TEXT("TerrainPass"); }
	virtual void Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk) override;
};

