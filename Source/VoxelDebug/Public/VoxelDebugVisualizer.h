// VoxelDebugVisualizer.h
//
// Purpose:
//   Cube-per-visible-voxel debug renderer. Answers "does the generated
//   world actually look right" before any time is spent on real greedy
//   meshing/rendering. Explicitly NOT the production renderer - no greedy
//   meshing, no LOD, no texture atlas, no face culling beyond a cheap
//   "skip fully-buried voxels" check for instance-count sanity.
//
//   This is an AActor with an AActor's usual overhead, which is normally
//   against ADR-001 (chunks aren't Actors) - that ADR is about the
//   PRODUCTION chunk representation. A debug tool that exists to be looked
//   at in the editor, not streamed at scale, is a legitimate exception;
//   don't copy this pattern into VoxelStreaming/VoxelWorldSubsystem later.
//
// Responsibilities:
//   - Run the generation pipeline for a small grid of chunks
//   - Spawn one InstancedStaticMeshComponent per distinct block ID seen,
//     one instance per solid voxel that has at least one air-exposed face
//   - Nothing else - no interaction, no collision, no persistence
//
// Thread ownership: Game Thread only. GenerateAndVisualize() runs
// generation synchronously and inline - deliberately not dispatched
// through FVoxelScheduler, since blocking the editor briefly while a small
// debug preview builds is an acceptable tradeoff for a tool that exists to
// be run occasionally by hand, not every frame.
//
// Dependencies: Engine (AActor, UInstancedStaticMeshComponent), VoxelCore,
//   VoxelStorage, VoxelGeneration, VoxelAssets (for optional biome-driven runs).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelCoreTypes.h"
#include "VoxelDebugVisualizer.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UVoxelBiomeDefinition;

UCLASS()
class VOXELDEBUG_API AVoxelDebugVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AVoxelDebugVisualizer();

	/** Seed passed to the generation pipeline. Change and re-run to compare worlds. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	int32 WorldSeed = 1234;

	/** Voxels per chunk edge. Kept independent of UVoxelRuntimeSettings so this tool works without a full world subsystem setup. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "4", ClampMax = "64"))
	int32 ChunkSize = 32;

	/** How many chunks to generate along X and Y, centered on the origin chunk. 1 = just chunk (0,0,*). */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "1", ClampMax = "6"))
	int32 ChunkRadiusXY = 2;

	/** How many chunks to generate along Z, starting from chunk Z=0 upward. Increase if terrain height pushes outside a single chunk. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ChunkCountZ = 3;

	/** World-space size of one voxel cube, in Unreal units. Default 100 matches the engine's basic cube mesh's native size. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "1"))
	float VoxelWorldSize = 100.0f;

	/** Optional per-block-ID material override, purely for visual distinction (e.g. 1=stone gray, 3=grass green). Unset IDs use DefaultMaterial. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TMap<int32, TObjectPtr<UMaterialInterface>> BlockMaterials;

	/** Used for any block ID not present in BlockMaterials. Defaults to the engine's basic material if left unset. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	/** Optional biomes to pass into generation - leave empty to use TerrainPass's built-in flat-layer fallback. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TArray<TObjectPtr<UVoxelBiomeDefinition>> Biomes;

	/** Runs generation for the configured chunk grid and (re)builds the cube visualization. Safe to call repeatedly - clears previous instances first. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug")
	void GenerateAndVisualize();

	/** Removes all spawned instanced mesh components without regenerating. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug")
	void ClearVisualization();

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	// One ISMC per distinct block ID actually placed, so BlockMaterials can
	// color them independently. Rebuilt from scratch each GenerateAndVisualize call.
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> BlockIdToComponent;

	UInstancedStaticMeshComponent* GetOrCreateComponentForBlock(int32 BlockId);
};
