// VoxelBiomeDefinition.h
//
// Purpose:
//   Data-asset definition of a biome. VoxelGeneration's BiomePass (Phase 2)
//   selects one of these per column based on climate/temperature/humidity
//   fields, then the rest of the pipeline (terrain, vegetation, structures)
//   reads its rules from this asset - no per-biome engine code.
//
// Responsibilities: pure data.
// Thread ownership: same as any UDataAsset (Game Thread load, safe
//   read-only access from worker threads during generation).
// Dependencies: Core, CoreUObject, Engine, VoxelCore, VoxelBlockDefinition.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "VoxelBlockDefinition.h"
#include "VoxelBiomeDefinition.generated.h"

/** One layer of a biome's terrain (e.g. "top 1 block grass, next 4 dirt, rest stone"). */
USTRUCT(BlueprintType)
struct FVoxelTerrainLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ToolTip = "Block definition used for this layer."))
	TSoftObjectPtr<UVoxelBlockDefinition> Block;

	/** Layer thickness in voxels. Ignored (fills to bedrock) for the last layer in the list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "1", ToolTip = "Thickness of this layer in voxels. The last layer in the list fills downwards to bedrock."))
	int32 ThicknessVoxels = 1;
};

UCLASS(BlueprintType)
class VOXELASSETS_API UVoxelBiomeDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ToolTip = "Display name of this biome."))
	FText DisplayName;

	/** Climate selection range this biome applies to, both in [0,1]. Used by BiomePass to pick a biome per column. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Normalized temperature range [0, 1] where this biome appears."))
	FVector2D TemperatureRange = FVector2D(0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Normalized humidity range [0, 1] where this biome appears."))
	FVector2D HumidityRange = FVector2D(0.0f, 1.0f);

	/** Ordered top-down. Last entry fills downward until the next reserved layer/bedrock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ToolTip = "Ordered top-down terrain layers (e.g. Grass -> Dirt -> Stone)."))
	TArray<FVoxelTerrainLayer> TerrainLayers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience", meta = (ToolTip = "Ambient color tint applied in this biome."))
	FLinearColor AmbientTint = FLinearColor::White;

	/** Gameplay-facing tags (weather, audio zone, spawn rules) - left generic so gameplay code defines their own meaning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay", meta = (ToolTip = "Gameplay tags associated with this biome for audio, weather, or spawn rules."))
	FGameplayTagContainer BiomeTags;

	/** Chance [0,1] per eligible column for VegetationPass to place a vegetation entry from this biome. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation", meta = (ClampMin = "0", ClampMax = "1", ToolTip = "Vegetation placement density [0, 1]."))
	float VegetationDensity = 0.1f;
};
