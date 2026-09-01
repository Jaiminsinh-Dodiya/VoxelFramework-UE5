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

	UPROPERTY(EditDefaultsOnly, Category = "Climate")
	float Frequency = 0.001f;

	UPROPERTY(EditDefaultsOnly, Category = "Climate")
	int32 TemperatureSeedOffset = 1000;

	UPROPERTY(EditDefaultsOnly, Category = "Climate")
	int32 HumiditySeedOffset = 2000;
};

/**
 * Wrapper for terrain settings to expose to Unreal reflection system.
 */
USTRUCT(BlueprintType)
struct VOXELASSETS_API FVoxelTerrainSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	int32 BaseHeight = 64;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	float HeightAmplitude = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	float BaseFrequency = 0.01f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	float Lacunarity = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	float Persistence = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback")
	TSoftObjectPtr<UVoxelBlockDefinition> FallbackStoneBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback")
	TSoftObjectPtr<UVoxelBlockDefinition> FallbackDirtBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback")
	TSoftObjectPtr<UVoxelBlockDefinition> FallbackGrassBlock;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Fallback", meta = (ClampMin = "1"))
	int32 FallbackDirtDepth = 4;
};

/**
 * Wrapper for cave settings to expose to Unreal reflection system.
 */
USTRUCT(BlueprintType)
struct VOXELASSETS_API FVoxelCaveSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Caves")
	bool bEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ClampMax = "1.0"))
	float CarveThreshold = 0.58f;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled"))
	float DensityFrequency = 0.045f;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled", ClampMin = "0"))
	int32 SurfaceProtectionDepth = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Caves", meta = (EditCondition = "bEnabled"))
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

	UPROPERTY(EditDefaultsOnly, Category = "Climate")
	FVoxelClimateSettings Climate;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	FVoxelTerrainSettings Terrain;

	UPROPERTY(EditDefaultsOnly, Category = "Caves")
	FVoxelCaveSettings Caves;

	/**
	 * Resolves soft pointers to block IDs and builds a plain worker-safe configuration.
	 * Must be called on the Game Thread.
	 */
	FVoxelGenerationConfig ToRuntimeConfig(const UVoxelBlockRegistry* Registry) const;
};
