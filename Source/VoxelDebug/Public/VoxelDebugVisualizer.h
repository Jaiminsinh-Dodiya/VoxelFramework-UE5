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
#include "Containers/Ticker.h"
#include "VoxelDebugVisualizer.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UVoxelMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UVoxelBiomeDefinition;
class UVoxelBlockRegistry;
class FVoxelChunk;

UENUM(BlueprintType)
enum class EVoxelDiagnosticMode : uint8
{
	ModeB_VoxelRenderingOn UMETA(DisplayName = "Mode B: Voxel Rendering ON"),
	ModeA_Baseline         UMETA(DisplayName = "Mode A: Baseline (Framework OFF)"),
	ModeC_CpuOnly          UMETA(DisplayName = "Mode C: CPU-Only (No Render Components)"),
	ModeD_StaticWorld      UMETA(DisplayName = "Mode D: Static World (Streaming Frozen)"),
	ModeE_StreamingStress  UMETA(DisplayName = "Mode E: Streaming Stress")
};

UCLASS()
class VOXELDEBUG_API AVoxelDebugVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AVoxelDebugVisualizer();

	/** World seed passed into the generation pipeline for all preview modes. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	int32 WorldSeed = 1234;

	/** Chunk dimensions (XYZ). Must match UVoxelRuntimeSettings in real gameplay; exposed here so you can test smaller/larger chunks in isolation. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "4", ClampMax = "64"))
	int32 ChunkSize = 32;

	/** Number of chunks to generate in +/- X and +/- Y around the visualizer actor. Radius 1 = 3x3 = 9 chunks; Radius 2 = 5x5 = 25 chunks. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "0", ClampMax = "8"))
	int32 ChunkRadiusXY = 1;

	/** Number of chunks to stack along Z (from chunk Z=0 up to Z=ChunkCountZ-1). Matches WorldHeightInChunks. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ChunkCountZ = 2;

	/** World-space size of one voxel in Unreal units (cm). 100 = 1 meter per voxel (standard Minecraft scale). */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	float VoxelWorldSize = 100.0f;

	/** Per-block material overrides (MaterialLayerIndex -> Material). */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TMap<int32, TSoftObjectPtr<UMaterialInterface>> BlockMaterials;

	/** Fallback material used for any block without an explicit material assigned. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	/** Optional biomes to pass into generation - leave empty to use TerrainPass's built-in flat-layer fallback. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	TArray<TObjectPtr<UVoxelBiomeDefinition>> Biomes;

	/** If true, mesh-mode components get simple collision enabled so you can walk/fly through the preview in PIE. Off by default to keep the debug tool cheap. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug")
	bool bEnableCollisionInMeshPreview = false;

	/** Current diagnostic mode for isolating performance bottlenecks. */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug|Performance")
	EVoxelDiagnosticMode ActiveDiagnosticMode = EVoxelDiagnosticMode::ModeB_VoxelRenderingOn;

	/** Toggle dynamic shadow casting on voxel mesh components (useful to isolate Virtual Shadow Map non-nanite marking cost). */
	UPROPERTY(EditAnywhere, Category = "Voxel Debug|Performance")
	bool bVoxelCastShadows = true;

	/** Cube-per-visible-voxel preview. Validates generation (terrain shape, caves, biomes). Clears any existing visualization first. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Cube Preview")
	void GenerateAndVisualize();

	/** Real FVoxelMesher output via UProceduralMeshComponent. Validates meshing (greedy merge quality, hidden faces, baked AO). Clears any existing visualization first. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Mesh Preview")
	void GenerateAndVisualizeMeshed();

	/** Same FVoxelMesher output as GenerateAndVisualizeMeshed, but through the REAL production UVoxelMeshComponent/FVoxelMeshSceneProxy rather than PMC. Compare against the PMC mode to sanity-check the custom renderer. Clears any existing visualization first. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Real Renderer Preview")
	void GenerateAndVisualizeRendered();

	/** Exercises the REAL UVoxelWorldSubsystem async pipeline. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|World Subsystem Test")
	void RequestChunksViaSubsystem();

	/** Validates subsystem generated results block-by-block. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|World Subsystem Test")
	void ValidateSubsystemResults();

	/** Removes all spawned components (all three modes) without regenerating. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug")
	void ClearVisualization();

	/** Starts a 10 Hz on-screen live diagnostics overlay showing FPS, thread timings, frame pacing percentiles, and streaming telemetry. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance")
	void StartPerformanceDiagnostics();

	/** Stops the diagnostics ticker and removes all on-screen diagnostic lines. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance")
	void StopPerformanceDiagnostics();

	/** Mode A: Disables voxel rendering and streaming to measure the engine's pure baseline cost. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance|Modes")
	void ApplyModeA_Baseline();

	/** Mode B: Standard voxel generation + meshing + rendering + streaming. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance|Modes")
	void ApplyModeB_VoxelRenderingOn();

	/** Mode C: CPU-only generation and meshing (no render components or GPU work), isolating CPU throughput. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance|Modes")
	void ApplyModeC_CpuOnly();

	/** Mode D: Static voxel world (streaming updates frozen), isolating steady-state rendering cost. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance|Modes")
	void ApplyModeD_StaticWorld();

	/** Mode E: Streaming stress (forces rapid chunk churn). */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance|Modes")
	void ApplyModeE_StreamingStress();

	/** Toggles shadow casting across all active voxel mesh components. */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance")
	void ToggleVoxelShadows();

	/** Resets accumulated frame pacing statistics (P95, P99, min/max, spike counters). */
	UFUNCTION(CallInEditor, Category = "Voxel Debug|Performance")
	void ResetDiagnosticStats();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

private:
	static constexpr uint64 DiagnosticsKeyBase = 0x564F58454C000000ull;
	static constexpr int32 DiagnosticsLineCount = 12;
	static constexpr int32 MaxFrameHistorySamples = 1000;

	uint64 GetDiagnosticsKey(int32 LineIndex) const
	{
		const uint64 InstanceSalt = (static_cast<uint64>(GetTypeHash(GetUniqueID())) & 0xFFFFull) << 16;
		return DiagnosticsKeyBase | InstanceSalt | static_cast<uint64>(LineIndex & 0xFFFF);
	}

	FTSTicker::FDelegateHandle DiagnosticsTickerHandle;
	bool bDiagnosticsRunning = false;

	TArray<float> FrameTimeHistory;
	int32 FramesOver16Ms = 0;
	int32 FramesOver33Ms = 0;
	int32 FramesOver50Ms = 0;
	float MinFrameTimeMs = FLT_MAX;
	float MaxFrameTimeMs = 0.0f;
	float TotalFrameTimeAccumMs = 0.0f;
	int32 TotalFramesSampled = 0;

	float CalculatePercentile(float Percentile) const;

	bool DiagnosticsTick(float DeltaTime);
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
