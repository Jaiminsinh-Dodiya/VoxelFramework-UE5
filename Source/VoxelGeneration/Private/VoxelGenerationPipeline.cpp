// VoxelGenerationPipeline.cpp
//
// Pass order note: Climate must run before Biome (Biome selects using
// Temperature/Humidity), Biome before Terrain (Terrain reads the selected
// biome's layer list), and Cave after Terrain (Cave carves through blocks
// Terrain already placed, and reads TerrainHeight for surface protection).
// River/Structure/Vegetation are NOT yet implemented - deliberately left
// out of the default pipeline rather than added as empty stubs, so "what's
// actually running" stays honest. Add them here as they're built.

#include "VoxelGenerationPipeline.h"
#include "IVoxelGenerationPass.h"
#include "Passes/ClimatePass.h"
#include "Passes/BiomePass.h"
#include "Passes/TerrainPass.h"
#include "Passes/CavePass.h"

FVoxelGenerationPipeline::FVoxelGenerationPipeline()
{
	Passes.Add(MakeUnique<FClimatePass>());
	Passes.Add(MakeUnique<FBiomePass>());
	Passes.Add(MakeUnique<FTerrainPass>());
	Passes.Add(MakeUnique<FCavePass>());
}

FVoxelGenerationPipeline::~FVoxelGenerationPipeline() = default;

void FVoxelGenerationPipeline::GenerateChunk(
	int32 WorldSeed,
	const FVoxelChunkCoordinate& Coordinate,
	int32 ChunkSize,
	const UVoxelBlockRegistry* BlockRegistry,
	const TArray<const UVoxelBiomeDefinition*>& AvailableBiomes,
	FVoxelChunk& OutChunk,
	const FVoxelGenerationConfig* GenerationConfig) const
{
	FVoxelGenerationContext Context;
	Context.WorldSeed = WorldSeed;
	Context.ChunkCoordinate = Coordinate;
	Context.ChunkSize = ChunkSize;
	Context.BlockRegistry = BlockRegistry;
	Context.AvailableBiomes = AvailableBiomes;
	Context.Config = GenerationConfig;
	Context.InitColumns();

	for (const TUniquePtr<IVoxelGenerationPass>& Pass : Passes)
	{
		Pass->Execute(Context, OutChunk);
	}
}

