// VoxelDebugVisualizer.h
//
// Purpose:
//   Three debug preview modes for validating generation + meshing +
//   rendering before/while VoxelRendering matures:
//
//   1. Cube preview (GenerateAndVisualize) - one cube per visible voxel.
//      Validates generation output (terrain shape, caves, biomes) cheaply.
//   2. PMC mesh preview (GenerateAndVisualizeMeshed) - runs the REAL
//      FVoxelMesher and displays its output via UProceduralMeshComponent.
//      Validates meshing output (greedy merging, hidden-face removal,
//      baked AO) independent of whether the real renderer works yet.
//   3. Real renderer preview (GenerateAndVisualizeRendered) - runs the
//      SAME FVoxelMesher output through the actual production
//      UVoxelMeshComponent / FVoxelMeshSceneProxy. This is the first mode
//      that exercises the real rendering path rather than a debug
//      substitute - compare its output directly against mode 2 (same mesh
//      data, two different consumers) to sanity-check the custom scene
//      proxy: if they look identical, that's strong evidence
//      FVoxelMeshSceneProxy is correct; if they diverge, the difference
//      points at exactly what to debug.
//
//   Modes 1 and 2 are NOT the production renderer. UProceduralMeshComponent
//   is an explicit, deliberate exception to ADR-004 ("meshing and
//   rendering are separate modules, no PMC") - that ADR governs the
//   PRODUCTION rendering path (VoxelRendering). Mode 3 uses the real
//   production component and is the only one of the three whose visual
//   output should be trusted as representative of what players will see.
//
// Responsibilities: generate a small chunk grid, display it three ways.
//   Nothing else - no interaction; mesh/render modes enable basic
//   collision only so you can walk/fly through it in PIE if desired.
//
// Thread ownership: Game Thread only, synchronous. Same tradeoff as the
//   cube mode - acceptable for a tool run occasionally by hand, not a
//   pattern for production streaming code.
//
// Dependencies: Engine (AActor, UInstancedStaticMeshComponent),
//   ProceduralMeshComponent (debug-only, see above), VoxelCore,
//   VoxelStorage, VoxelGeneration, VoxelAssets, VoxelMeshing, VoxelRendering.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelCoreTypes.h"
#include "VoxelDebugVisualizer.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UVoxelMeshComponent;
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

	/** Same FVoxelMesher output as GenerateAndVisualizeMeshed, but through the REAL production UVoxelMeshComponent/FVoxelMeshSceneProxy rather than PMC. Compare against the PMC mode to sanity-check the custom renderer. Clears any existing visualization first. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Real Renderer Preview")
	void GenerateAndVisualizeRendered();

	/** Removes all spawned components (all three modes) without regenerating. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug")
	void ClearVisualization();

private:
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	// Cube mode: one ISMC per distinct block ID actually placed.
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UInstancedStaticMeshComponent>> BlockIdToComponent;

	// PMC mesh mode: one PMC per generated chunk (simplest correct approach -
	// see .cpp for why this isn't further optimized, it's a debug tool).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> MeshPreviewComponents;

	// Real renderer mode: one UVoxelMeshComponent per generated chunk, same
	// per-chunk granularity as the PMC mode for a fair visual comparison.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelMeshComponent>> RenderedPreviewComponents;

	UInstancedStaticMeshComponent* GetOrCreateComponentForBlock(int32 BlockId);

	/**
	 * Shared generation step used by both preview modes: builds the local
	 * block registry (if Biomes is non-empty) and generates the configured
	 * chunk grid. Returns the registry (may be nullptr) via OutRegistry.
	 */
	TMap<FVoxelChunkCoordinate, TUniquePtr<FVoxelChunk>> GenerateChunkGrid(UVoxelBlockRegistry*& OutRegistry);
};
