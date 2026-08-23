// VoxelDebugVisualizer.h
//
// Purpose:
//   Two debug preview modes for validating generation + meshing before any
//   time is spent on the real VoxelRendering module:
//
//   1. Cube preview (GenerateAndVisualize) - one cube per visible voxel.
//      Validates generation output (terrain shape, caves, biomes) cheaply.
//   2. Mesh preview (GenerateAndVisualizeMeshed) - runs the REAL
//      FVoxelMesher and displays its actual output via
//      UProceduralMeshComponent. Validates meshing output (greedy merging,
//      hidden-face removal, baked AO) - this is the first time the actual
//      production geometry algorithm's output is visible, as opposed to
//      just passing automation tests.
//
//   Neither mode is the production renderer. UProceduralMeshComponent is
//   an explicit, deliberate exception to ADR-004 ("meshing and rendering
//   are separate modules, no PMC") - that ADR governs the PRODUCTION
//   rendering path (VoxelRendering, not built yet). This debug tool exists
//   specifically to look at FVoxelMeshData's output before that module is
//   written, and PMC is the fastest way to put arbitrary triangle soup on
//   screen for a look-and-verify pass. Do not copy this pattern into
//   VoxelRendering - see ADR.md and the module's own header comment for why.
//
// Responsibilities: generate a small chunk grid, display it two ways.
//   Nothing else - no interaction, no collision (mesh mode enables basic
//   collision only so you can walk/fly through it in PIE if desired).
//
// Thread ownership: Game Thread only, synchronous. Same tradeoff as the
//   cube mode - acceptable for a tool run occasionally by hand, not a
//   pattern for production streaming code.
//
// Dependencies: Engine (AActor, UInstancedStaticMeshComponent),
//   ProceduralMeshComponent (debug-only, see above), VoxelCore,
//   VoxelStorage, VoxelGeneration, VoxelAssets, VoxelMeshing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelCoreTypes.h"
#include "VoxelDebugVisualizer.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UVoxelBiomeDefinition;
class UVoxelBlockRegistry;
class FVoxelChunk;

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

	/** World-space size of one voxel cube/unit, in Unreal units. Default 100 matches the engine's basic cube mesh's native size and gives FVoxelMesher's 1-unit-per-voxel output a sensible scale. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "1"))
	float VoxelWorldSize = 100.0f;

	/**
	 * Per-block/material-ID material override, used by BOTH preview modes.
	 * Cube mode keys this by raw FVoxelBlockId. Mesh mode keys it by the
	 * MaterialId FVoxelMesher resolved (which, with no block registry
	 * configured below, IS the raw FVoxelBlockId - see FVoxelMesher::
	 * ResolveMaterialId) - so in the common no-registry case, the same map
	 * works identically for both modes.
	 */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TMap<int32, TObjectPtr<UMaterialInterface>> BlockMaterials;

	/** Used for any block/material ID not present in BlockMaterials. Unset = engine default material. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	/** Optional biomes to pass into generation - leave empty to use TerrainPass's built-in flat-layer fallback. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TArray<TObjectPtr<UVoxelBiomeDefinition>> Biomes;

	/** If true, mesh-mode components get simple collision enabled so you can walk/fly through the preview in PIE. Off by default to keep the debug tool cheap. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	bool bEnableCollisionInMeshPreview = false;

	/** Cube-per-visible-voxel preview. Validates generation (terrain shape, caves, biomes). Clears any existing visualization first. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Cube Preview")
	void GenerateAndVisualize();

	/** Real FVoxelMesher output via UProceduralMeshComponent. Validates meshing (greedy merge quality, hidden faces, baked AO). Clears any existing visualization first. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Mesh Preview")
	void GenerateAndVisualizeMeshed();

	/** Removes all spawned components (both modes) without regenerating. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug")
	void ClearVisualization();

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	// Cube mode: one ISMC per distinct block ID actually placed.
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> BlockIdToComponent;

	// Mesh mode: one PMC per generated chunk (simplest correct approach -
	// see .cpp for why this isn't further optimized, it's a debug tool).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> MeshPreviewComponents;

	UInstancedStaticMeshComponent* GetOrCreateComponentForBlock(int32 BlockId);

	/**
	 * Shared generation step used by both preview modes: builds the local
	 * block registry (if Biomes is non-empty) and generates the configured
	 * chunk grid. Returns the registry (may be nullptr) via OutRegistry.
	 */
	TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> GenerateChunkGrid(UVoxelBlockRegistry*& OutRegistry);
};
