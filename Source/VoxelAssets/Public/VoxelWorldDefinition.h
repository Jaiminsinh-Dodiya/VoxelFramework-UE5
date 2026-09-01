// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelWorldDefinition.generated.h"

class UVoxelGenerationDefinition;
class UVoxelBiomeDefinition;
class UVoxelStreamingPreset;
class UMaterialInterface;

/**
 * UVoxelWorldDefinition
 * 
 * Central composition asset for a voxel world.
 * References other specialized assets, where each referenced asset owns its own configuration domain.
 * The WorldDefinition itself only owns world identity (name, seed, scale) and material mappings.
 */
UCLASS(BlueprintType)
class VOXELASSETS_API UVoxelWorldDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// Identity

	UPROPERTY(EditDefaultsOnly, Category = "World")
	FText WorldName;

	UPROPERTY(EditDefaultsOnly, Category = "World")
	int32 WorldSeed = 1234;

	UPROPERTY(EditDefaultsOnly, Category = "World", meta = (ClampMin = "1.0"))
	float VoxelWorldSize = 100.0f;

	// Asset References — composition, NOT duplication

	UPROPERTY(EditDefaultsOnly, Category = "Generation")
	TSoftObjectPtr<UVoxelGenerationDefinition> GenerationDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Biomes")
	TArray<TSoftObjectPtr<UVoxelBiomeDefinition>> Biomes;

	UPROPERTY(EditDefaultsOnly, Category = "Streaming")
	TSoftObjectPtr<UVoxelStreamingPreset> StreamingPreset;

	UPROPERTY(EditDefaultsOnly, Category = "Physics", meta = (ToolTip = "Assign a UVoxelPhysicsPreset asset"))
	TSoftObjectPtr<UDataAsset> PhysicsPreset;

	UPROPERTY(EditDefaultsOnly, Category = "Rendering")
	TMap<int32, TSoftObjectPtr<UMaterialInterface>> BlockMaterials;

	UPROPERTY(EditDefaultsOnly, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> DefaultMaterial;
};
