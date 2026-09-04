# Voxel Framework

A high-performance, modular, production-ready voxel engine plugin for Unreal Engine 5.7.

## Plugin Metadata
- **Name:** Voxel Framework
- **Version:** 2.4.0 (Framework Authoring & Developer UX)
- **Engine:** Unreal Engine 5.7
- **Author:** Jaimin
- **Architecture:** 12 independent, strictly decoupled modules

## Module Status

| Layer | Status | Description |
|---|---|---|
| **VoxelCore** | ✅ Complete | Leaf value types, coordinate math, handles, worker-safe runtime structs (`FVoxelGenerationConfig`) |
| **VoxelRuntime** | ✅ Complete | UE::Tasks asynchronous scheduler wrapper with terminal completion & bounded history |
| **VoxelMath** | ✅ Complete | Deterministic 2D/3D FastNoise SIMD gradient & FBM noise generators |
| **VoxelAssets** | ✅ Complete | `UVoxelWorldDefinition`, `UVoxelGenerationDefinition`, `UVoxelStreamingPreset`, `UVoxelConfigValidator` |
| **VoxelStorage** | ✅ Complete | Compact 16-bit voxel storage, pooled chunk stores, and worker lease lifecycles |
| **VoxelGeneration** | ✅ Complete | 4-pass data-driven pipeline: Climate → Biome → Terrain → 3D Caves (toggleable) |
| **VoxelMeshing** | ✅ Complete | Greedy mesher + baked AO + 36-byte vertex format + neighbor boundary culling |
| **VoxelRendering** | ✅ Complete | Custom `UVoxelMeshComponent` & `FVoxelMeshSceneProxy` with $O(1)$ bounds |
| **VoxelPhysics** | ✅ Complete | Worker collision builder, `UVoxelCollisionComponent`, `UVoxelPhysicsPreset`, Chaos async cooking |
| **VoxelWorld** | ✅ Complete | Async world subsystem, Blueprint query APIs, component pooling, shutdown barrier |
| **VoxelStreaming** | ✅ Complete | 4-band distance manager, runtime distance controls, adaptive budget |
| **VoxelDebug** | ✅ Complete | 5 visual preview modes + 10 Hz real-time performance telemetry HUD |
| **Serialization** | ⏸️ Decision Checkpoint | Optional terrain diff saving; skipped for v1 if world regenerates deterministically |

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

    VoxelPhysics --> VoxelCore;
    VoxelPhysics --> VoxelStorage;
    VoxelPhysics --> VoxelAssets;
    VoxelPhysics --> VoxelRuntime;
    
    VoxelWorld --> VoxelCore;
    VoxelWorld --> VoxelRuntime;
    VoxelWorld --> VoxelAssets;
    VoxelWorld --> VoxelStorage;
    VoxelWorld --> VoxelGeneration;
    VoxelWorld --> VoxelMeshing;
    VoxelWorld --> VoxelRendering;
    VoxelWorld --> VoxelPhysics;

    VoxelStreaming --> VoxelCore;
    VoxelStreaming --> VoxelRuntime;
    VoxelStreaming --> VoxelWorld;
    VoxelStreaming --> VoxelAssets;
    
    VoxelDebug --> VoxelGeneration;
    VoxelDebug --> VoxelMeshing;
    VoxelDebug --> VoxelStorage;
    VoxelDebug --> VoxelAssets;
    VoxelDebug --> VoxelRendering;
    VoxelDebug --> VoxelPhysics;
    VoxelDebug --> VoxelWorld;
    VoxelDebug --> VoxelStreaming;
```

## Architectural Highlights & Invariants

1. **Strict Plugin / Game Boundary**: VoxelFramework is a generic, reusable plugin technology. Game-specific storylines, handcrafted landmark reservations, and quests live outside the plugin and consume it.
2. **Data-Driven Configuration Precedence (ADR-007)**: Strict 4-tier precedence: `Project Settings` → `World Definition` → `Presets` → `Runtime Blueprint Overrides`.
3. **Worker-Safe Runtime Structs**: All designer-facing UDataAssets (`UVoxelGenerationDefinition`) are translated into plain, immutable C++ structs (`FVoxelGenerationConfig`) at initialization on the Game Thread before worker dispatch, eliminating UObject contention and GC races.
4. **Dedicated Physical Collision Architecture (ADR-006)**: Visual geometry (`UVoxelMeshComponent`, up to `RenderDistance=14`) and physical collision geometry (`UVoxelCollisionComponent`, up to `SimulationDistance=4`) are strictly decoupled.
5. **Conservative Blueprint APIs**: Spatial queries (`TryGetBlockAtWorldPosition`, `TryIsSolidAtWorldPosition`) use explicit residency checks (`bool Try...`) and never trigger silent synchronous generation or frame drops.
6. **Scheduler Terminal Completion & Bounded History**: Every submitted job has exactly one terminal completion path. `OnComplete` (and external lease cleanup) is guaranteed to execute across all job lifecycles.
7. **World Shutdown Barrier**: `UVoxelWorldSubsystem::Deinitialize` waits on all in-flight worker tasks (`WaitForAllTasks`) before resetting storage.
8. **Neighbor Lifetime Safety & Boundary Culling**: Meshing and collision acquire worker leases on all cardinal neighbors and only read `Ready` neighbors.
9. **Compact 36-Byte Vertices**: `FVoxelMeshVertex` utilizes single-precision `FVector3f`, `FVector2f`, and `FColor` (36 bytes vs 80-byte double-precision legacy), slashing GPU bandwidth by ~55%.
10. **Precomputed Relative Offsets & Single-Pass Streaming**: `UVoxelStreamingManager` translates pre-sorted relative offsets in $O(N)$ with 0 heap allocations and 0 runtime sorting.

## Testing (55 Passing Automation Tests)

The plugin leverages Unreal's Automation Testing framework with 55 passing tests ensuring complete subsystem integrity:

| Suite | Module | Covers |
|---|---|---|
| `Voxel.Assets.BiomeLayerResolution` | VoxelAssets | Soft-pointer biome layer resolution & caching |
| `Voxel.Configuration.BiomeDefinitionValidation` | VoxelAssets | Validation checks for biome terrain layers |
| `Voxel.Configuration.BlockDefinitionValidation` | VoxelAssets | Duplicate and reserved block ID validation |
| `Voxel.Configuration.GenerationConfigFromDefinition` | VoxelAssets | DataAsset to plain runtime struct conversion |
| `Voxel.Configuration.StreamingPresetApply` | VoxelAssets | Streaming preset property values & application |
| `Voxel.Configuration.ValidationErrors` | VoxelAssets | World definition missing generation asset error check |
| `Voxel.Configuration.ValidationWarnings` | VoxelAssets | Invalid generation height range warning check |
| `Voxel.Configuration.WorldDefinitionDefaults` | VoxelAssets | Default seed, scale, and null soft pointer safety |
| `Voxel.Generation.Cave.AirRatioLogged` | VoxelGeneration | Bounded air carving ratios |
| `Voxel.Generation.Cave.BoundaryContinuity` | VoxelGeneration | Cross-chunk continuous cave seams (>75% correlation) |
| `Voxel.Generation.Cave.DeterministicHash` | VoxelGeneration | Hash reproducibility |
| `Voxel.Generation.Cave.SurfaceProtected` | VoxelGeneration | Prevents cave carving on surface grass layers |
| `Voxel.Generation.ConfigBaseHeightShift` | VoxelGeneration | Terrain base height shifting verifies solid voxel count |
| `Voxel.Generation.ConfigCavesToggleable` | VoxelGeneration | Caves disabled toggle (`bEnabled = false`) verification |
| `Voxel.Generation.ConfigDeterminism` | VoxelGeneration | Generation config + seed determinism |
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
| `Voxel.Physics.Cave` | VoxelPhysics | Subterranean cave collision geometry & internal face emission |
| `Voxel.Physics.CookFailure` | VoxelPhysics | Empty data / cook failure handling & delegate notification |
| `Voxel.Physics.Deterministic` | VoxelPhysics | Bit-exact reproducible collision vertices and indices |
| `Voxel.Physics.EmptyChunk` | VoxelPhysics | Fast-path for all-air chunks (0 vertices, 0 triangles) |
| `Voxel.Physics.FlatSurface` | VoxelPhysics | Greedy quad merging on terrain collision (observed >80% triangle reduction) |
| `Voxel.Physics.MissingNeighbor` | VoxelPhysics | Graceful fallback to air on missing/null neighbors |
| `Voxel.Physics.NeighborArrivalDuringCook` | VoxelPhysics | Neighbor arrival during build culls boundary faces safely |
| `Voxel.Physics.NeighborBoundary` | VoxelPhysics | Cross-chunk shared boundary collision face culling |
| `Voxel.Physics.NeighborUnloadDuringCook` | VoxelPhysics | Worker lease protection prevents neighbor memory recycling during build |
| `Voxel.Physics.NonCollidableFilter` | VoxelPhysics | Filtering blocks where `bGeneratesCollision = false` |
| `Voxel.Physics.OutwardWindingNormals` | VoxelPhysics | Winding order verification ensuring floor normals point UP (+Z) |
| `Voxel.Physics.SingleVoxel` | VoxelPhysics | Unit cube 6-quad collision shape and analytical bounds |
| `Voxel.Physics.Slope` | VoxelPhysics | Stepped slope collision geometry and multi-quad bounds |
| `Voxel.Physics.StaleRevision` | VoxelPhysics | Revision counter stale cook rejection and component lifecycle |
| `Voxel.Physics.UnloadDuringCook` | VoxelPhysics | Abort and cleanup safety during in-flight async cook |
| `Voxel.Rendering.ComponentBookkeeping` | VoxelRendering | SetMeshData/ClearMeshData, bounds, proxy lifecycle |
| `Voxel.Storage.CreateStoreModifyQuery` | VoxelStorage | Chunk lifecycle, handle stale-rejection, gen-write vs gameplay-edit diff tracking |
| `Voxel.Streaming.BandClassification` | VoxelStreaming | Pure function distance classification |
| `Voxel.Streaming.CancellationStateTransition` | VoxelStreaming | Deterministic worker cancellation |
| `Voxel.Streaming.DesiredCoordinates` | VoxelStreaming | Z-clamped sorting & bounding |
| `Voxel.Streaming.DistancePriorityMapping` | VoxelStreaming | Scheduler priority mapping |
| `Voxel.Streaming.LongRunStress` | VoxelStreaming | 1,000-iteration boundary churn, slot stability & memory verification |
| `Voxel.Streaming.NeighborLifetimeSafety` | VoxelStreaming | Neighbor worker lease retention during unload and delayed recycling |
| `Voxel.Streaming.SchedulerBoundedHistory` | VoxelStreaming | 2,000-job historical bounded retention |
| `Voxel.Streaming.SchedulerTerminalCompletion` | VoxelStreaming | Terminal completion on queued, running, duplicate, and post-completion cancellations |
| `Voxel.Streaming.SpawnChunkCollisionWithoutMovement` | VoxelStreaming | Immediate collision request for initial spawn chunk without requiring movement |
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
    ├── VoxelPhysics/
    ├── VoxelWorld/
    ├── VoxelStreaming/
    └── VoxelDebug/
```

## Roadmap

Storage ✅ → Assets ✅ → Biomes ✅ → Terrain ✅ → Caves ✅ → Meshing ✅ → Rendering ✅ → World Subsystem ✅ → Streaming ✅ → VoxelPhysics V1 ✅ → **Framework Authoring & Developer UX ✅** → Gameplay Systems

## Getting Started

1. Copy `VoxelFramework/` into your project's `Plugins/` folder.
2. Regenerate project files for your `.uproject`.
3. Build your project (Development Editor, Win64/Android/iOS supported).
4. Enable the **Voxel Framework** plugin in the editor.
5. Place an `AVoxelDebugVisualizer` actor in your scene to inspect generation, meshing, or run live benchmarks.

## Configuration & Authoring

The framework offers data-driven authoring assets:
1. **World Definition (`UVoxelWorldDefinition`)**: Primary composite asset referencing generation parameters, biomes, streaming presets, physics presets, and materials.
2. **Generation Definition (`UVoxelGenerationDefinition`)**: Modular data asset containing `FVoxelClimateSettings`, `FVoxelTerrainSettings`, and `FVoxelCaveSettings`.
3. **Streaming Preset (`UVoxelStreamingPreset`)**: Tune distance bands (`Simulation`, `Render`, `Generation`, `Persistence`) and frame budgets (`StreamingBudgetMs`).
4. **Physics Preset (`UVoxelPhysicsPreset`)**: In `VoxelPhysics`, configure collision fidelity and async cooking.
5. **Config Validator (`UVoxelConfigValidator`)**: Run `UVoxelConfigValidator::ValidateWorldDefinition` in editor utility widgets or scripts to audit configuration integrity before PIE/packaging.
6. **Project Settings**: Set `DefaultWorldDefinition` in **Project Settings → Plugins → Voxel World**.

## License
TBD — Internal Development / Pre-release.

