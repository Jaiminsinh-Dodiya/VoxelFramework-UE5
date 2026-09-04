// ClimatePass.h
//
// Purpose: writes Temperature/Humidity per column via independent large-
//   scale noise fields, which BiomePass then reads to select a biome.
// Thread ownership: worker thread only.
// Dependencies: VoxelMath.

#pragma once

#include "IVoxelGenerationPass.h"

class FClimatePass : public IVoxelGenerationPass
{
public:
	virtual const TCHAR* GetPassName() const override { return TEXT("ClimatePass"); }
	virtual void Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk) override;
};

