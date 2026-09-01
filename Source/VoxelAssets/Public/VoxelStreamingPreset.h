// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelStreamingPreset.generated.h"

/**
 * UVoxelStreamingPreset
 * 
 * Preset for streaming distance bands and GT budget caps.
 * Designed to be assigned to UVoxelWorldDefinition or applied at runtime.
 * Does NOT own distance ordering policy (distances are independent).
 */
UCLASS(BlueprintType)
class VOXELASSETS_API UVoxelStreamingPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1"))
	int32 SimulationDistance = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1"))
	int32 RenderDistance = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1"))
	int32 GenerationDistance = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1"))
	int32 PersistenceDistance = 12;

	UPROPERTY(EditDefaultsOnly, Category = "Budget", meta = (ClampMin = "0.1"))
	float StreamingBudgetMs = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Budget", meta = (ClampMin = "1"))
	int32 MaxMeshFinalizationsPerTick = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Budget", meta = (ClampMin = "1"))
	int32 MaxCollisionFinalizationsPerTick = 4;
};
