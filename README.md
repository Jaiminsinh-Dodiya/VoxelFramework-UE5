# Voxel Framework

A high-performance, modular, production-ready voxel engine plugin for Unreal Engine 5.7.

## Plugin Metadata
- **Name:** Voxel Framework
- **Version:** 2.1.0 (Production Candidate)
- **Engine:** Unreal Engine 5.7
- **Author:** Jaimin
- **Architecture:** 11 independent, strictly decoupled modules

## Module Status

| Layer | Status | Description |
|---|---|---|
| **VoxelCore** | ✅ Complete | Leaf value types, coordinate math, handles, and job interfaces |
| **VoxelRuntime** | ✅ Complete | UE::Tasks asynchronous scheduler wrapper and project runtime settings |
| **VoxelMath** | ✅ Complete | Deterministic 2D/3D FastNoise SIMD gradient & FBM noise generators |
| **VoxelAssets** | ✅ Complete | Block definitions, biome layers, and runtime block registry |
| **VoxelStorage** | ✅ Complete | Compact 16-bit voxel storage, pooled chunk stores, and worker leases |
| **VoxelGeneration** | ✅ Complete | 4-pass reentrant pipeline: Climate → Biome → Terrain → 3D Caves |
| **VoxelMeshing** | ✅ Complete | Greedy mesher + baked AO + 36-byte vertex format + neighbor boundary culling |
| **VoxelRendering** | ✅ Complete | Custom `UVoxelMeshComponent` & `FVoxelMeshSceneProxy` with $O(1)$ bounds |
| **VoxelWorld** | ✅ Complete | Async world subsystem, component pooling, and MPSC finalization queue |
| **VoxelStreaming** | ✅ Complete | 4-band distance manager, precomputed offsets, single-pass evaluation, adaptive budget |
| **VoxelDebug** | ✅ Complete | 5 visual preview modes + 10 Hz real-time performance telemetry HUD |
| **Serialization** | ⬜ Next Phase | Diff-based save/load using modified chunk diffs (ADR-005) |

## Modules & Dependencies

```mermaid
graph TD;
    VoxelRuntime --> VoxelCore;
    VoxelMath --> VoxelCore;
    VoxelAssets --> VoxelCore;
    VoxelStorage --> VoxelCore;
    VoxelStorage --> VoxelRuntime;
    
    VoxelGeneration --> VoxelMath;
    VoxelGeneration --> VoxelAssets;
    VoxelGeneration --> VoxelStorage;
    VoxelGeneration --> VoxelRuntime;
    
    VoxelMeshing --> VoxelStorage;
    VoxelMeshing --> VoxelAssets;
    VoxelMeshing --> VoxelRuntime;
    
    VoxelRendering --> VoxelMeshing;
    
    VoxelWorld --> VoxelCore;
    VoxelWorld --> VoxelRuntime;
    VoxelWorld --> VoxelAssets;
    VoxelWorld --> VoxelStorage;
    VoxelWorld --> VoxelGeneration;
    VoxelWorld --> VoxelMeshing;
    VoxelWorld --> VoxelRendering;

    VoxelStreaming --> VoxelCore;
    VoxelStreaming --> VoxelRuntime;
    VoxelStreaming --> VoxelWorld;
    
    VoxelDebug --> VoxelGeneration;
    VoxelDebug --> VoxelMeshing;
    VoxelDebug --> VoxelStorage;
    VoxelDebug --> VoxelAssets;
    VoxelDebug --> VoxelRendering;
    VoxelDebug --> VoxelWorld;
    VoxelDebug --> VoxelStreaming;
```

## Architectural Highlights & Invariants

1. **Strict Plugin / Game Boundary**: VoxelFramework is a generic, reusable plugin technology. Game-specific storylines, handcrafted landmark reservations, and quests live outside the plugin and consume it.
2. **Compact 36-Byte Vertices**: `FVoxelMeshVertex` utilizes single-precision `FVector3f`, `FVector2f`, and `FColor` (36 bytes vs 80-byte double-precision legacy), slashing GPU bandwidth and cache footprint by ~55%.
3. **Neighbor-Aware Meshing & Seam Culling**: `FVoxelMesher` inspects adjacent resident chunks to eliminate internal boundary faces, triggering asynchronous remeshes on neighbor arrival/unload without regenerating voxel data.
4. **Worker-Side Vertex Transforms & Analytical Bounds**: Vertex world transformation and bounding box calculation happen concurrently on worker threads. Game Thread component registration runs in $O(1)$ time with 0 vertex iterations.
5. **Precomputed Relative Offsets & Single-Pass Streaming**: `UVoxelStreamingManager` translates pre-sorted relative offsets in $O(N)$ with 0 heap allocations and 0 runtime sorting on chunk crossings, performing unloads and visibility in a single unified pass.
6. **Adaptive Streaming Budget**: Automatically scales down streaming slice on warm/hot frames to protect 60/30 FPS frame pacing on mobile hardware.
7. **Thread-Safe Component Pooling & Worker Leases**: Reuses `UVoxelMeshComponent` instances to eliminate GC garbage generation, and uses `AcquireWorkerLease` to prevent use-after-free corruption during async generation.

## Testing (23 Passing Automation Tests)

The plugin leverages Unreal's Automation Testing framework with 23 passing tests ensuring complete subsystem integrity:

| Suite | Module | Covers |
|---|---|---|
| `Voxel.Assets.BiomeLayerResolution` | VoxelAssets | Soft-pointer biome layer resolution & caching |
| `Voxel.Generation.Cave.AirRatioLogged` | VoxelGeneration | Bounded air carving ratios |
| `Voxel.Generation.Cave.BoundaryContinuity` | VoxelGeneration | Cross-chunk continuous cave seams (>75% correlation) |
| `Voxel.Generation.Cave.DeterministicHash` | VoxelGeneration | Hash reproducibility |
| `Voxel.Generation.Cave.SurfaceProtected` | VoxelGeneration | Prevents cave carving on surface grass layers |
| `Voxel.Generation.DeterministicFromSeed` | VoxelGeneration | Seed determinism |
| `Voxel.Generation.PerfLog` | VoxelGeneration | Pipeline generation timing |
| `Voxel.Generation.TerrainRespectsBiomeLayers` | VoxelGeneration | Material layering |
| `Voxel.Meshing.AdjacentVoxelsMergeSameMaterial` | VoxelMeshing | Greedy quad merging |
| `Voxel.Meshing.AmbientOcclusionVariesByProximity` | VoxelMeshing | Corner AO darkening verified |
| `Voxel.Meshing.DeterministicOutput` | VoxelMeshing | Bit-exact remeshing output |
| `Voxel.Meshing.EmptyChunk` | VoxelMeshing | $O(1)$ all-air chunk fast-path |
| `Voxel.Meshing.MaterialBoundaryPreventsMerge` | VoxelMeshing | Multi-material boundary separation |
| `Voxel.Meshing.NeighborBoundaryCulling` | VoxelMeshing | 36-byte vertex layout & cross-chunk boundary quad culling |
| `Voxel.Meshing.PerfLog` | VoxelMeshing | Meshing timing |
| `Voxel.Meshing.SingleVoxel` | VoxelMeshing | Unit cube geometry validation |
| `Voxel.Rendering.ComponentBookkeeping` | VoxelRendering | SetMeshData/ClearMeshData, bounds, proxy lifecycle |
| `Voxel.Storage.CreateStoreModifyQuery` | VoxelStorage | Chunk lifecycle, handle stale-rejection, gen-write vs gameplay-edit diff tracking |
| `Voxel.Streaming.BandClassification` | VoxelStreaming | Pure function distance classification |
| `Voxel.Streaming.CancellationStateTransition` | VoxelStreaming | Deterministic worker cancellation |
| `Voxel.Streaming.DesiredCoordinates` | VoxelStreaming | Z-clamped sorting & bounding |
| `Voxel.Streaming.DistancePriorityMapping` | VoxelStreaming | Scheduler priority mapping |
| `Voxel.Streaming.StateMachineTransitions` | VoxelStreaming | Authoritative chunk state machine |
| `Voxel.Streaming.StorageWorkerLeaseLifecycle` | VoxelStreaming | Safe async chunk leasing without use-after-free |
| `Voxel.World.RequestUnloadBookkeeping` | VoxelWorld | Subsystem request/unload idempotency |

### VoxelDebug Visualizer Modes
The `AVoxelDebugVisualizer` actor supports 5 CallInEditor modes for rapid in-editor validation and profiling:
1. **ApplyModeA_Baseline:** Disables voxel rendering and streaming to measure pure engine baseline.
2. **ApplyModeB_VoxelRenderingOn:** Standard voxel generation + meshing + rendering + streaming.
3. **ApplyModeC_CpuOnly:** CPU-only generation and meshing isolation (render components and GPU work bypassed).
4. **ApplyModeD_StaticWorld:** Static world (streaming updates frozen) to isolate steady-state rendering and draw calls.
5. **ApplyModeE_StreamingStress:** Streaming stress testing under rapid traversal.

Includes a **10 Hz live telemetry overlay** reporting instant/min/max FPS, P95/P99 frame pacing, thread breakdowns (Game, Render, GPU), queue latencies, component pool metrics, and mobile target compliance.

## Project Structure
```text
VoxelFramework/
├── VoxelFramework.uplugin
├── README.md
├── Docs/
│   ├── ADR.md
│   ├── ARCHITECTURE.md
│   ├── API_REFERENCE.md
│   └── TODO.md
└── Source/
    ├── VoxelCore/
    ├── VoxelRuntime/
    ├── VoxelMath/
    ├── VoxelAssets/
    ├── VoxelStorage/
    ├── VoxelGeneration/
    │   └── Passes/
    ├── VoxelMeshing/
    ├── VoxelRendering/
    ├── VoxelWorld/
    ├── VoxelStreaming/
    └── VoxelDebug/
```

## Roadmap

Storage ✅ → Assets ✅ → Biomes ✅ → Terrain ✅ → Caves ✅ → Meshing ✅ → Rendering ✅ → World Subsystem ✅ → Streaming ✅ → **Serialization (next)** → Gameplay Systems

## Getting Started

1. Copy `VoxelFramework/` into your project's `Plugins/` folder.
2. Regenerate project files for your `.uproject`.
3. Build your project (Development Editor, Win64/Android/iOS supported).
4. Enable the **Voxel Framework** plugin in the editor.
5. Place an `AVoxelDebugVisualizer` actor in your scene to inspect generation, meshing, or run live benchmarks.

## Configuration

Two Project Settings panels are provided under Plugins:
1. **Voxel Framework** (`UVoxelRuntimeSettings`): Adjust ChunkSize, WorldHeightInChunks, streaming distances, generation budgets, and memory budgets.
2. **Voxel World** (`UVoxelWorldSettings`): Define WorldSeed, DefaultBiomes, VoxelWorldSize, BlockMaterials, and DefaultMaterial.

## License
TBD — Internal Development / Pre-release.
