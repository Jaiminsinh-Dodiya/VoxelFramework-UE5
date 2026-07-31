// CavePass.h
//
// Purpose:
//   Carves air pockets through already-placed terrain using a 3D density
//   field. Runs after TerrainPass (needs solid blocks to carve through) and
//   reads TerrainHeight from the column data TerrainPass wrote, so it can
//   protect the surface from being carved open.
//
// Responsibilities:
//   - Sample a 3D fBm density field per voxel, in world space (continuous
//     across chunk boundaries by construction - same reasoning as
//     TerrainPass/ClimatePass sampling world coordinates, not local ones)
//   - Carve (set to air) any currently-solid voxel whose density exceeds
//     the carve threshold, EXCEPT within SurfaceProtectionDepth voxels of
//     the terrain surface, so caves don't turn the surface into swiss cheese
//
// Thread ownership: worker thread only. No UObject access, no asset
//   loading, no Game Thread calls - pure math over already-resolved data
//   (Context.Columns' TerrainHeight, the chunk's own current block state).
// Dependencies: VoxelMath (3D noise) only, per the "pure pass" contract.

#pragma once

#include "IVoxelGenerationPass.h"

class FCavePass : public IVoxelGenerationPass
{
public:
	virtual const TCHAR* GetPassName() const override { return TEXT("CavePass"); }
	virtual void Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk) override;

private:
	static constexpr int32 NoiseOctaves = 3;
	static constexpr float DensityFrequency = 0.045f;
	static constexpr float CarveThreshold = 0.58f; // higher = fewer/smaller caves; density is roughly [0,1] after remap
	static constexpr int32 SurfaceProtectionDepth = 3; // voxels below TerrainHeight that are never carved
};
