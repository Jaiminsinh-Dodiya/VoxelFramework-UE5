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

	UPROPERTY(EditDefaultsOnly, Category = "World", meta = (ToolTip = "Human-readable name for this voxel world (e.g. 'Desert World', 'Cave World'). Used for display and debugging only."))
	FText WorldName;

	UPROPERTY(EditDefaultsOnly, Category = "World", meta = (ToolTip = "Master seed for deterministic world generation. Changing this produces an entirely different world. Two worlds with the same seed and generation definition produce identical terrain."))
	int32 WorldSeed = 1234;

	UPROPERTY(EditDefaultsOnly, Category = "World", meta = (ClampMin = "1.0", ToolTip = "Size of one voxel in Unreal units (cm). Default 100 = each voxel is 1 meter. Affects chunk world size: ChunkSize * VoxelWorldSize."))
	float VoxelWorldSize = 100.0f;

	// Asset References — composition, NOT duplication

	UPROPERTY(EditDefaultsOnly, Category = "Generation", meta = (ToolTip = "Reference to a UVoxelGenerationDefinition data asset that controls terrain shape, climate noise, cave carving, and all generation parameters. Create one via Content Browser > Miscellaneous > Data Asset > VoxelGenerationDefinition."))
	TSoftObjectPtr<UVoxelGenerationDefinition> GenerationDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Biomes", meta = (ToolTip = "Array of biome definition assets. Each biome defines block layering (grass depth, dirt depth, stone). Multiple biomes enable climate-driven biome selection."))
	TArray<TSoftObjectPtr<UVoxelBiomeDefinition>> Biomes;

	UPROPERTY(EditDefaultsOnly, Category = "Streaming", meta = (ToolTip = "Optional reference to a UVoxelStreamingPreset that configures streaming distance bands (Simulation, Render, Generation, Persistence) and frame budgets. If unset, project settings defaults are used."))
	TSoftObjectPtr<UVoxelStreamingPreset> StreamingPreset;

	UPROPERTY(EditDefaultsOnly, Category = "Physics", meta = (ToolTip = "Optional reference to a UVoxelPhysicsPreset (in VoxelPhysics module) that configures collision mode, async cooking, and collision profile. If unset, default physics settings are used."))
	TSoftObjectPtr<UDataAsset> PhysicsPreset;

	UPROPERTY(EditDefaultsOnly, Category = "Rendering", meta = (ToolTip = "Map of Block ID to Material. Assign a material for each block type (e.g. BlockID 1 = Grass Material, BlockID 2 = Dirt Material). Blocks without a mapping use the DefaultMaterial."))
	TMap<int32, TSoftObjectPtr<UMaterialInterface>> BlockMaterials;

	UPROPERTY(EditDefaultsOnly, Category = "Rendering", meta = (ToolTip = "Fallback material used for any block ID that doesn't have an entry in BlockMaterials. If unset, Unreal's default material is used."))
	TSoftObjectPtr<UMaterialInterface> DefaultMaterial;

};
