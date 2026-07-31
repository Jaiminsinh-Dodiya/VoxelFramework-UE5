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

private:
	// Deliberately low frequency + different noise "channel" (seed offset)
	// than TerrainPass so climate varies smoothly over large regions,
	// independent of local terrain shape.
	static constexpr float ClimateFrequency = 0.001f;
	static constexpr int32 TemperatureSeedOffset = 1000;
	static constexpr int32 HumiditySeedOffset = 2000;
};
