// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VoxelBlueprintLibrary.generated.h"

class UVoxelWorldSubsystem;
class UVoxelWorldDefinition;

/**
 * UVoxelBlueprintLibrary
 *
 * Static Blueprint function library for Voxel Framework.
 * These nodes can be called directly from ANY Blueprint graph (Actor, Character, PlayerController,
 * UserWidget, AnimInstance, Level Blueprint, etc.) without needing a manual Subsystem reference pin.
 */
UCLASS()
class VOXELWORLD_API UVoxelBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the Voxel World Subsystem for the current world.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self', Actor, Component, Widget).
	 * @return The active UVoxelWorldSubsystem instance, or nullptr if the world is invalid.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|World", meta = (WorldContext = "WorldContextObject", Keywords = "voxel world subsystem manager instance get"))
	static UVoxelWorldSubsystem* GetVoxelWorldSubsystem(const UObject* WorldContextObject);

	/**
	 * Converts an Unreal continuous 3D world position in centimeters (cm) to an integer Chunk Coordinate (X, Y, Z).
	 *
	 * Example:
	 *   If ChunkSize = 32 and VoxelWorldSize = 100cm (each chunk is 3200cm across),
	 *   a Character at Location (3500, 100, 200) maps to Chunk Coordinate (1, 0, 0).
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param WorldPosition The continuous 3D world position in centimeters (e.g. from GetActorLocation or LineTrace Hit Location).
	 * @return The integer chunk coordinate (X, Y, Z).
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Query", meta = (WorldContext = "WorldContextObject", Keywords = "world position to chunk coordinate coord locate player convert"))
	static FIntVector WorldPositionToChunkCoordinate(const UObject* WorldContextObject, const FVector& WorldPosition);

	/**
	 * Queries the 16-bit block ID at a given continuous world position.
	 *
	 * Threading & Safety:
	 *   - This is a non-blocking, memory-resident lookup.
	 *   - Returns true if the containing chunk is currently loaded in memory, populating OutBlockId.
	 *   - Returns false if the chunk is still generating, queued, or unloaded (will NEVER cause hitching or synchronous generation).
	 *   - Block ID 0 is reserved for Air (VoxelBlockId_Air).
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param WorldPosition The continuous 3D world position in centimeters (e.g. from GetActorLocation or LineTrace Hit Location).
	 * @param OutBlockId The resulting block ID at this voxel location (0 = Air, >0 = specific block definition).
	 * @return True if the chunk was loaded and OutBlockId is authoritative; false if the chunk was not resident.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Query", meta = (WorldContext = "WorldContextObject", Keywords = "get block id at world position query voxel type"))
	static bool TryGetBlockAtWorldPosition(const UObject* WorldContextObject, const FVector& WorldPosition, int32& OutBlockId);

	/**
	 * Queries whether the voxel at a given continuous world position is physically solid.
	 *
	 * Threading & Safety:
	 *   - This is a non-blocking, memory-resident lookup.
	 *   - Returns true if the containing chunk is currently loaded in memory, populating bOutIsSolid.
	 *   - Returns false if the chunk is still generating, queued, or unloaded.
	 *   - Air (Block ID 0) returns bOutIsSolid = false.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param WorldPosition The continuous 3D world position in centimeters (e.g. from GetActorLocation or LineTrace Hit Location).
	 * @param bOutIsSolid Outputs true if the block definition is marked solid (collidable/walkable), false for air or non-solid foliage.
	 * @return True if the chunk was loaded and bOutIsSolid is authoritative; false if the chunk was not resident.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Query", meta = (WorldContext = "WorldContextObject", Keywords = "is solid ground floor walkable at world position query voxel"))
	static bool TryIsSolidAtWorldPosition(const UObject* WorldContextObject, const FVector& WorldPosition, bool& bOutIsSolid);

	/**
	 * Returns true if the specified integer chunk coordinate is generated, meshed, and resident in memory.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param ChunkCoord The integer chunk coordinate (X, Y, Z).
	 * @return True if the chunk is ready in memory.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Chunk", meta = (WorldContext = "WorldContextObject", Keywords = "is chunk loaded ready resident in memory"))
	static bool IsChunkLoaded(const UObject* WorldContextObject, const FIntVector& ChunkCoord);

	/**
	 * Returns true if the specified integer chunk coordinate has active, cooked physical Chaos collision registered in the physics scene.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param ChunkCoord The integer chunk coordinate (X, Y, Z).
	 * @return True if character / physics collision is ready for this chunk.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Chunk", meta = (WorldContext = "WorldContextObject", Keywords = "is chunk collision ready cooked chaos physics"))
	static bool IsChunkCollisionReady(const UObject* WorldContextObject, const FIntVector& ChunkCoord);

	/**
	 * Returns the edge size of a single chunk in voxels (e.g. 32 voxels along X, Y, and Z).
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return Chunk edge size in voxels (typically 16, 32, or 64).
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|World", meta = (WorldContext = "WorldContextObject", Keywords = "get chunk size voxels edge dimension"))
	static int32 GetChunkSize(const UObject* WorldContextObject);

	/**
	 * Returns the active deterministic world seed used for terrain and biome procedural generation.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return The integer world seed.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|World", meta = (WorldContext = "WorldContextObject", Keywords = "get world seed random procedural deterministic"))
	static int32 GetWorldSeed(const UObject* WorldContextObject);

	/**
	 * Returns the world-space size of a single voxel cube in centimeters (default = 100.0 cm = 1 meter).
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return World scale in centimeters per voxel.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|World", meta = (WorldContext = "WorldContextObject", Keywords = "get voxel world size scale centimeters cm meter"))
	static float GetVoxelWorldSize(const UObject* WorldContextObject);

	/**
	 * Returns true if the Voxel World Subsystem is initialized and storage is allocated.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @return True if initialized.
	 */
	UFUNCTION(BlueprintPure, Category = "Voxel|World", meta = (WorldContext = "WorldContextObject", Keywords = "is voxel world initialized ready active"))
	static bool IsWorldInitialized(const UObject* WorldContextObject);

	/**
	 * Applies a new World Definition Data Asset to the world subsystem at runtime.
	 * Configures the world seed, voxel world size, generation definition, biomes, and material mappings.
	 *
	 * @param WorldContextObject Any object in the target world (e.g. 'Self').
	 * @param WorldDefinition The UVoxelWorldDefinition asset to apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|World", meta = (WorldContext = "WorldContextObject", Keywords = "apply load switch world definition preset config"))
	static void ApplyWorldDefinition(const UObject* WorldContextObject, const UVoxelWorldDefinition* WorldDefinition);
};
