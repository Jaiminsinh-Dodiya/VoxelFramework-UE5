// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCoreTypes.h"

/**
 * Plain C++ struct for runtime climate configuration.
 * Safe to pass by const reference to worker threads.
 */
struct FVoxelClimateConfig
{
	float Frequency = 0.001f;
	int32 TemperatureSeedOffset = 1000;
	int32 HumiditySeedOffset = 2000;
};

/**
 * Plain C++ struct for runtime terrain configuration.
 * Safe to pass by const reference to worker threads.
 */
struct FVoxelTerrainConfig
{
	int32 BaseHeight = 64;
	float HeightAmplitude = 40.0f;
	float BaseFrequency = 0.01f;
	int32 NoiseOctaves = 4;
	float Lacunarity = 2.0f;
	float Persistence = 0.5f;
	
	FVoxelBlockId FallbackStoneId = 1;
	FVoxelBlockId FallbackDirtId = 2;
	FVoxelBlockId FallbackGrassId = 3;
	int32 FallbackDirtDepth = 4;
};

/**
 * Plain C++ struct for runtime cave configuration.
 * Safe to pass by const reference to worker threads.
 */
struct FVoxelCaveConfig
{
	bool bEnabled = true;
	float CarveThreshold = 0.58f;
	float DensityFrequency = 0.045f;
	int32 NoiseOctaves = 3;
	int32 SurfaceProtectionDepth = 3;
	int32 CaveSeedOffset = 5000;
	float Lacunarity = 2.0f;
	float Persistence = 0.5f;
};

/**
 * Root configuration structure for voxel generation.
 *
 * This is a plain C++ struct with no UObject dependencies.
 * It is built from UVoxelGenerationDefinition at world initialization on the Game Thread.
 * It is safe to pass to background worker threads. Workers must never access UObjects directly.
 */
struct FVoxelGenerationConfig
{
	FVoxelClimateConfig Climate;
	FVoxelTerrainConfig Terrain;
	FVoxelCaveConfig Caves;
};

