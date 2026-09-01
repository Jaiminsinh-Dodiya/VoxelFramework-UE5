// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelStreamingBlueprintLibrary.generated.h"

class UVoxelStreamingManager;
class UVoxelStreamingPreset;

/**
 * UVoxelStreamingBlueprintLibrary
 *
 * Static Blueprint function library for Voxel Framework Streaming & LOD management.
 * Call these functions directly from ANY Blueprint (Option Menus, Characters, GameMode, Widgets).
 */
UCLASS()
class VOXELSTREAMING_API UVoxelStreamingBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the active Voxel Streaming Manager for the current world.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return The active UVoxelStreamingManager instance, or nullptr if unavailable.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "voxel streaming manager subsystem get"))
	static UVoxelStreamingManager* GetVoxelStreamingManager(const UObject* WorldContextObject);

	/**
	 * Applies a UVoxelStreamingPreset data asset to update all distance bands and frame budgets at once.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param Preset The UVoxelStreamingPreset asset to apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "apply streaming preset distance budget graphics settings"))
	static void ApplyStreamingPreset(const UObject* WorldContextObject, const UVoxelStreamingPreset* Preset);

	/**
	 * Sets the active Render Distance (in chunks).
	 * Visible geometry is rendered up to this radius around the player.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param NewRenderDistance Radius in chunks (e.g. 8 = 17x17 grid). Must be >= 1.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "set render distance view graphics option chunks"))
	static void SetStreamingRenderDistance(const UObject* WorldContextObject, int32 NewRenderDistance);

	/**
	 * Gets the current Render Distance (in chunks).
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return Current radius in chunks.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "get render distance view graphics chunks"))
	static int32 GetStreamingRenderDistance(const UObject* WorldContextObject);

	/**
	 * Sets the active Simulation Distance (in chunks).
	 * Physical terrain collision and character interaction are generated up to this radius.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param NewSimulationDistance Radius in chunks (e.g. 4 = 9x9 grid). Must be <= RenderDistance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "set simulation distance collision physics chunks"))
	static void SetStreamingSimulationDistance(const UObject* WorldContextObject, int32 NewSimulationDistance);

	/**
	 * Gets the current Simulation Distance (in chunks).
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return Current collision radius in chunks.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "get simulation distance collision physics chunks"))
	static int32 GetStreamingSimulationDistance(const UObject* WorldContextObject);

	/**
	 * Sets the target Game Thread streaming decision budget in milliseconds.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param NewBudgetMs Frame budget in milliseconds (e.g. 1.5 ms).
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "set streaming budget ms milliseconds frame time performance"))
	static void SetStreamingBudgetMs(const UObject* WorldContextObject, float NewBudgetMs);

	/**
	 * Gets the current Game Thread streaming decision budget in milliseconds.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return Frame budget in milliseconds.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Streaming", meta = (WorldContext = "WorldContextObject", Keywords = "get streaming budget ms milliseconds frame time performance"))
	static float GetStreamingBudgetMs(const UObject* WorldContextObject);

	/**
	 * Freezes or unfreezes dynamic streaming updates (used for static rendering benchmarks and debug isolation).
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param bFrozen True to freeze streaming updates; false to resume normal tracking.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Development", meta = (WorldContext = "WorldContextObject", Keywords = "set freeze streaming static world debug benchmark"))
	static void SetStreamingFrozen(const UObject* WorldContextObject, bool bFrozen);

	/**
	 * Returns true if dynamic streaming updates are currently frozen.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return True if frozen.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Development", meta = (WorldContext = "WorldContextObject", Keywords = "is streaming frozen debug benchmark"))
	static bool IsStreamingFrozen(const UObject* WorldContextObject);
};
