// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelGenerationDefinition.h"
#include "VoxelBlockRegistry.h"

FVoxelGenerationConfig UVoxelGenerationDefinition::ToRuntimeConfig(const UVoxelBlockRegistry* Registry) const
{
	FVoxelGenerationConfig Config;

	// Copy Climate Settings
	Config.Climate.Frequency = Climate.Frequency;
	Config.Climate.TemperatureSeedOffset = Climate.TemperatureSeedOffset;
	Config.Climate.HumiditySeedOffset = Climate.HumiditySeedOffset;

	// Copy Terrain Settings
	Config.Terrain.BaseHeight = Terrain.BaseHeight;
	Config.Terrain.HeightAmplitude = Terrain.HeightAmplitude;
	Config.Terrain.BaseFrequency = Terrain.BaseFrequency;
	Config.Terrain.NoiseOctaves = Terrain.NoiseOctaves;
	Config.Terrain.Lacunarity = Terrain.Lacunarity;
	Config.Terrain.Persistence = Terrain.Persistence;
	Config.Terrain.FallbackDirtDepth = Terrain.FallbackDirtDepth;

	// Resolve fallback block definitions if set
	if (const UVoxelBlockDefinition* Stone = Terrain.FallbackStoneBlock.LoadSynchronous())
	{
		Config.Terrain.FallbackStoneId = static_cast<FVoxelBlockId>(Stone->BlockId);
	}
	if (const UVoxelBlockDefinition* Dirt = Terrain.FallbackDirtBlock.LoadSynchronous())
	{
		Config.Terrain.FallbackDirtId = static_cast<FVoxelBlockId>(Dirt->BlockId);
	}
	if (const UVoxelBlockDefinition* Grass = Terrain.FallbackGrassBlock.LoadSynchronous())
	{
		Config.Terrain.FallbackGrassId = static_cast<FVoxelBlockId>(Grass->BlockId);
	}

	// Copy Cave Settings
	Config.Caves.bEnabled = Caves.bEnabled;
	Config.Caves.CarveThreshold = Caves.CarveThreshold;
	Config.Caves.DensityFrequency = Caves.DensityFrequency;
	Config.Caves.NoiseOctaves = Caves.NoiseOctaves;
	Config.Caves.SurfaceProtectionDepth = Caves.SurfaceProtectionDepth;
	Config.Caves.CaveSeedOffset = Caves.CaveSeedOffset;
	Config.Caves.Lacunarity = 2.0f;
	Config.Caves.Persistence = 0.5f;

	return Config;
}

