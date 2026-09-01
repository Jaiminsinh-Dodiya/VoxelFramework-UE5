// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelGenerationConfig.h"
#include "VoxelBlockDefinition.h"
#include "VoxelGenerationDefinition.generated.h"

class UVoxelBlockRegistry;

/**
 * Wrapper for climate settings to expose to Unreal reflection system.
 */
USTRUCT(BlueprintType)
struct VOXELASSETS_API FVoxelClimateSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Climate", meta = (ToolTip = "Frequency of the climate noise map. Lower values = larger biome regions, higher values = more varied biomes. Default: 0.001."))
	float Frequency = 0.001f;

	UPROPERTY(EditDefaultsOnly, Category = "Climate", meta = (ToolTip = "Seed offset added to WorldSeed for the temperature noise channel. Change this to shift the temperature map independently. Default: 1000."))
	int32 TemperatureSeedOffset = 1000;

	UPROPERTY(EditDefaultsOnly, Category = "Climate", meta = (ToolTip = "Seed offset added to WorldSeed for the humidity noise channel. Change this to shift the humidity map independently. Default: 2000."))
	int32 HumiditySeedOffset = 2000;
};

/**
 * Wrapper for terrain settings to expose to Unreal reflection system.
 */
USTRUCT(BlueprintType)
struct VOXELASSETS_API FVoxelTerrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ToolTip = "Base terrain height in voxels. The terrain surface oscillates around this height. Higher values raise the entire world. Must be within [0, ChunkSize * WorldHeightInChunks). Default: 64."))
	int32 BaseHeight = 64;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ToolTip = "Maximum height variation in voxels above and below BaseHeight. Higher values = more dramatic hills and valleys. Default: 40."))
	float HeightAmplitude = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ToolTip = "Base frequency of the terrain height noise. Lower values = smoother, rolling terrain. Higher values = more rugged, smaller features. Default: 0.01."))
	float BaseFrequency = 0.01f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ClampMin = "1", ClampMax = "8", ToolTip = "Number of noise octaves layered for terrain height (fractal Brownian motion). More octaves add finer detail but cost more CPU. Range: 1-8. Default: 4."))
	int32 NoiseOctaves = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ToolTip = "Frequency multiplier between successive noise octaves. Higher values make each octave add progressively finer detail. Default: 2.0."))
	float Lacunarity = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ToolTip = "Amplitude multiplier between successive noise octaves. Lower values make higher octaves contribute less. Default: 0.5."))
	float Persistence = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback", meta = (ToolTip = "Block definition used for deep underground stone when no biome is available. Resolved to a Block ID at initialization."))
	TSoftObjectPtr<UVoxelBlockDefinition> FallbackStoneBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback", meta = (ToolTip = "Block definition used for the dirt layer between stone and grass when no biome is available."))
	TSoftObjectPtr<UVoxelBlockDefinition> FallbackDirtBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback", meta = (ToolTip = "Block definition used for the surface grass layer when no biome is available."))
	TSoftObjectPtr<UVoxelBlockDefinition> FallbackGrassBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback", meta = (ClampMin = "1", ToolTip = "Number of voxels below the surface that use the dirt block before transitioning to stone. Default: 4."))
	int32 FallbackDirtDepth = 4;
};

/**
 * Wrapper for cave settings to expose to Unreal reflection system.
 */
USTRUCT(BlueprintType)
struct VOXELASSETS_API FVoxelCaveSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (ToolTip = "Enable or disable cave carving entirely. When false, no underground cavities are generated. Default: true."))
	bool bEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Noise threshold above which voxels are carved into air to form caves. Higher values = smaller, rarer caves. Lower values = larger, more frequent caves. Range: 0.0-1.0. Default: 0.58."))
	float CarveThreshold = 0.58f;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ToolTip = "Frequency of the 3D cave density noise. Lower values = larger cave networks, higher values = smaller, more numerous caves. Default: 0.045."))
	float DensityFrequency = 0.045f;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "8", ToolTip = "Number of noise octaves for cave density. More octaves add finer cave detail. Range: 1-8. Default: 3."))
	int32 NoiseOctaves = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ClampMin = "0", ToolTip = "Number of voxels below the terrain surface where cave carving is prevented. Protects the grass/dirt surface from cave holes. 0 = no protection. Default: 3."))
	int32 SurfaceProtectionDepth = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ToolTip = "Seed offset added to WorldSeed for the cave noise channel. Change this to shift cave locations independently of terrain. Default: 5000."))
	int32 CaveSeedOffset = 5000;

};

/**
 * Designer-facing Data Asset for voxel generation definitions.
 * 
 * Provides a UI-friendly representation of generation settings in the editor.
 * Converts to worker-safe FVoxelGenerationConfig at runtime on the Game Thread.
 */
UCLASS(BlueprintType)
class VOXELASSETS_API UVoxelGenerationDefinition : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, Category = "Climate", meta = (ToolTip = "Climate noise settings controlling biome region sizes and seed offsets for temperature/humidity channels."))
	FVoxelClimateSettings Climate;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ToolTip = "Terrain height and shape settings including base height, noise octaves, amplitude, and fallback block types."))
	FVoxelTerrainSettings Terrain;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (ToolTip = "Cave generation settings. Toggle caves on/off, control cave size and frequency, and set surface protection depth."))
	FVoxelCaveSettings Caves;

	/**
	 * Resolves soft pointers to block IDs and builds a plain worker-safe configuration.
	 * Must be called on the Game Thread.
	 */
	FVoxelGenerationConfig ToRuntimeConfig(const UVoxelBlockRegistry* Registry) const;
};
