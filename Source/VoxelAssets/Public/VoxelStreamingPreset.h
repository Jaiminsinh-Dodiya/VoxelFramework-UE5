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
	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1", ToolTip = "Maximum distance (in chunks) from the viewer at which chunks have physical collision. Characters and physics objects only collide with terrain within this radius. Default: 4 chunks."))
	int32 SimulationDistance = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1", ToolTip = "Maximum distance (in chunks) from the viewer at which chunk meshes are visible. Chunks beyond this distance are hidden but may still be generated. Default: 8 chunks."))
	int32 RenderDistance = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1", ToolTip = "Maximum distance (in chunks) from the viewer at which new chunks are generated and meshed. Should be >= RenderDistance so chunks are ready before becoming visible. Default: 10 chunks."))
	int32 GenerationDistance = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Distances", meta = (ClampMin = "1", ToolTip = "Maximum distance (in chunks) from the viewer at which chunks are kept in memory. Chunks beyond this distance are fully unloaded. Should be >= GenerationDistance to avoid thrashing. Default: 12 chunks."))
	int32 PersistenceDistance = 12;

	UPROPERTY(EditDefaultsOnly, Category = "Budget", meta = (ClampMin = "0.1", ToolTip = "Maximum time budget (in milliseconds) per frame for streaming chunk load/unload operations. Lower values reduce frame hitching but slow down chunk loading. Default: 1.5 ms."))
	float StreamingBudgetMs = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Budget", meta = (ClampMin = "1", ToolTip = "Maximum number of chunk meshes finalized (applied to render components) per tick. Limits GPU upload spikes. Default: 4."))
	int32 MaxMeshFinalizationsPerTick = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Budget", meta = (ClampMin = "1", ToolTip = "Maximum number of collision shapes finalized (cooked and applied to physics) per tick. Limits Chaos physics cooking spikes. Default: 4."))
	int32 MaxCollisionFinalizationsPerTick = 4;

};
