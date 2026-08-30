# TODO / Future Scope

Living document. Ordered roughly by priority/dependency, not by difficulty — some "easy" items are listed late because nothing needs them yet.

See [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) §9 for the current honest list of what's deliberately not built, and [`Docs/ADR.md`](Docs/ADR.md) for decisions already frozen that future work must respect.

> **World/Game Design checkpoint still open for Region/Island systems** (see `Docs/ARCHITECTURE.md` §8). Nothing below "Paused" should be started until those are decided. `VoxelRendering` and `VoxelWorld` are now complete and were unaffected by the checkpoint, exactly as predicted.

---

## Just completed: VoxelRendering ✅

- [x] Custom `UVoxelMeshComponent` (`UMeshComponent`) + `FVoxelMeshSceneProxy` (`FPrimitiveSceneProxy`) — NOT `ProceduralMeshComponent`, per ADR-004
- [x] `FStaticMeshVertexBuffers`/`FLocalVertexFactory`, uploaded via `ENQUEUE_RENDER_COMMAND`
- [x] Proper vertex buffer upload and per-section draw calls with material assignment
- [x] GPU resource cleanup on destruction
- [x] Automation test: `Voxel.Rendering.ComponentBookkeeping` — SetMeshData/ClearMeshData lifecycle, material count, bounding box
- [x] **Visual validation** — extended `VoxelDebug` with `GenerateAndVisualizeRendered` mode that renders real `UVoxelMeshComponent` output side-by-side with the PMC preview for comparison
- [ ] **Known gap, deliberately deferred**: no frustum culling or LOD (distance-based mesh swap) — deferred to `VoxelStreaming`
- [ ] **Known gap, deliberately deferred**: no texture atlas + single material optimization — future mobile optimization pass
- [ ] **Known gap, deliberately deferred**: no async mesh upload (currently synchronous) — future optimization

## Just completed: VoxelWorld ✅

- [x] `UVoxelWorldSubsystem` (`UWorldSubsystem`) owning the real `FVoxelChunkStore` instance for a world
- [x] Calls `UVoxelBlockRegistry::PrecacheBiomeLayers` once at Initialize, not per-caller
- [x] `UVoxelWorldSettings` (`UDeveloperSettings`) for world seed, default biomes, voxel world size, block materials
- [x] Dispatches chunk generation AND meshing through `FVoxelScheduler` as one worker-thread job
- [x] Marshals results back to Game Thread via `AsyncTask(ENamedThreads::GameThread)` to create/update `UVoxelMeshComponent`
- [x] `AVoxelWorldRenderActor` as transient host for dynamically-attached mesh components
- [x] Idempotent `RequestChunk` — re-requesting same coordinate returns existing handle, no second job
- [x] `UnloadChunk` removes storage and rendering
- [x] `FindChunk` / `IsChunkReady` read-only accessors
- [x] Automation test: `Voxel.World.RequestUnloadBookkeeping` — request/unload lifecycle, idempotency
- [x] **Visual validation** — `VoxelDebug` integration test: `RequestChunksViaSubsystem` + `ValidateSubsystemResults` (48/48 ALL PASSED)
- [ ] **Known gap, deliberately deferred**: no job cancellation for in-flight work — deferred to `VoxelStreaming`
- [ ] **Known gap, deliberately deferred**: `ChunkMeshComponents` uses `TWeakObjectPtr` in a non-UPROPERTY `TMap` keyed by plain `FVoxelChunkCoordinate` (not USTRUCT) — GC safety via actor Outer, not reflection

## Just completed: VoxelStreaming (Phase 6.1 & 6.2 Performance & Hitch Elimination) ✅

- [x] Distance band classification pure functions (`EVoxelStreamingBand`, `ClassifyChunkDistance`, `ComputeDesiredCoordinates`)
- [x] `UVoxelStreamingManager` (`UTickableWorldSubsystem`) tracking player viewer position and streaming distance bands
- [x] **Chunk Lifetime & Storage Safety**: Implemented explicit worker leases (`AcquireWorkerLease` / `ReleaseWorkerLease` in `FVoxelChunkStore`). Chunk memory is NEVER recycled to `FreeSlotIndices` while an active worker holds a lease, eliminating use-after-free/data race during async work.
- [x] **Authoritative State Machine**: Added `EVoxelChunkState` (`Unloaded`, `Queued`, `Generating`, `Meshing`, `PendingFinalize`, `Ready`, `Unloading`).
- [x] **Budget-Limited Game Thread Finalization Queue**: Replaced unthrottled `AsyncTask(GameThread)` burst completions with `TQueue<FVoxelCompletedMeshItem, EQueueMode::Mpsc>` in `UVoxelWorldSubsystem`, draining within `RenderSubmissionBudgetMs` (default 1.0ms, max 4 chunks/tick).
- [x] **Queue Efficiency**: Converted $O(N)$ array shifts (`RemoveAt(0)`) to $O(1)$ cursor indexing (`PendingRequestIndex++`).
- [x] **Component Pooling (Stage A)**: Added GC-rooted `ComponentPool` on `UVoxelWorldSubsystem` to reuse unrendered `UVoxelMeshComponent` instances across chunk unloads and loads, eliminating `NewObject` / `DestroyComponent` spikes and GC allocation overhead.
- [x] **Distance-Aware Priority Scheduling (Stage B)**: Chunks are dispatched with priorities matching their distance band (`Critical` for simulation, `High` for render, `Normal` for generation prefetch, `Low` for background).
- [x] **Change-Driven Visibility (Stage C)**: Eliminated per-tick loops over managed chunk arrays. Visibility is strictly change-driven on chunk boundary transitions or distance setting changes.
- [x] **Runtime Distance Controls (Stage D)**: Added `SetRenderDistance`, `SetSimulationDistance`, `SetGenerationDistance`, `SetPersistenceDistance`, and `SetStreamingBudgetMs` with budget-safe progressive re-evaluation.
- [x] **Finalization Queue Latency Telemetry (Stage E)**: Added monotonic queue wait latency tracking (Avg, P50, P95, P99, Max, Oldest age).
- [x] **Live Performance Diagnostics & Profiling Modes**:
  - Mode A (Baseline / Framework OFF): Measures pure engine baseline cost.
  - Mode B (Voxel Rendering ON): Standard voxel generation + rendering + streaming.
  - Mode C (CPU Generation/Meshing Isolation): Measures generation + greedy meshing on CPU without render component / GPU creation.
  - Mode D (Static World / Streaming Frozen): Freezes dynamic streaming to isolate steady-state rendering cost.
  - Mode E (Streaming Stress): Stresses rapid boundary traversal and churn.
  - Live 12-line HUD displaying FPS, frame pacing percentiles, thread timings, component pool metrics, queue latency percentiles, and VSM shadow caster telemetry.
  - VSM Dynamic Shadow toggle (`bVoxelCastShadows` / `SetCastShadows`).
- [x] **Automation Tests**:
  - `Voxel.Streaming.BandClassification`
  - `Voxel.Streaming.DesiredCoordinates`
  - `Voxel.Streaming.CancellationStateTransition`
  - `Voxel.Streaming.StorageWorkerLeaseLifecycle`
  - `Voxel.Streaming.StateMachineTransitions`
  - `Voxel.Streaming.DistancePriorityMapping`
## Just completed: Phase 6.3 Mobile Scalability Hardening ✅

- [x] **6.3.1 Diagnostic Integrity**:
  - Unique message key hashing per `AVoxelDebugVisualizer` instance.
  - Preview geometry cleanup (`ClearVisualization`) on Mode A (Baseline), Mode B (Voxel ON), and Mode C (CPU Only) to prevent manual preview components from skewing real telemetry.
  - Fully verified `ResetDiagnosticStats` for clean profiling windows.
- [x] **6.3.2 Low-Risk CPU & Memory Improvements**:
  - **Compact Vertex Format (`FVoxelMeshVertex` 36 Bytes)**: `FVector3f Position`, `FVector3f Normal`, `FVector2f UV`, `FColor Color` (measured 36 bytes vs former 80 bytes $\rightarrow$ ~55% reduction in vertex bandwidth).
  - **Worker-Side Vertex Transformation**: Applied world-space chunk origin and scale on worker threads.
  - **Analytical Bounds**: Calculated `Bounds` on worker thread directly into `FVoxelMeshData`, making Game Thread `UVoxelMeshComponent::UpdateLocalBounds` an instantaneous $O(1)$ operation (zero vertex loops).
  - **Stateless Pipeline Sharing**: Reused `FVoxelGenerationPipeline` safely across concurrent worker tasks (zero `MakeUnique` allocations per chunk).
- [x] **6.3.3 Neighbor-Aware Meshing & Lifecycle Remeshing**:
  - `FVoxelMesher` queries `FVoxelNeighborChunks` (6 cardinal neighbors) to cull redundant internal boundary quads between touching solid chunks.
  - Missing neighbors fall back cleanly to air (no visual holes or cracks).
  - `EmptyChunk` fast-path skips greedy meshing sweeps entirely when `Chunk.IsEmpty()`.
  - **Neighbor Arrival & Unload Remeshing**: When a chunk becomes `Ready` or `Unloads`, adjacent resident chunks are scheduled for lightweight asynchronous remeshing to maintain seamless geometry without interior waste.
- [x] **6.3.4 Deep Streaming Optimizations (14-Chunk Scale)**:
  - **Precomputed Relative Offsets**: Precomputed sorted `CachedRelativeOffsets` at initialization and distance settings changes, eliminating $O(\text{volume} \log \text{volume})$ rebuilds and heap allocations on chunk crossings.
  - **Single-Pass Evaluation**: Merged `PendingUnloads` collection and visibility updates into one pass over `ManagedCoordinates`.
  - **Zero Duplicate Distances**: Split `GetPriorityForDistance(Dist)` to calculate Chebyshev distance only once in request loops.
  - **Throttled Clock Polling**: Reduced `FPlatformTime::Seconds()` queries to once every 8 iterations.
  - **Cached Chunk Coordinate Factors**: Cached `InvChunkWorldEdgeSize` for $O(1)$ multiplication in `WorldToChunkCoordinate`.
- [x] **Automation Test Suite (23/23 Passing)**:
  - Added `Voxel.Meshing.NeighborBoundaryCulling` validating exact 36-byte layout and cross-chunk internal boundary face culling.

## Just completed: Phase 6.4 Final Concurrency, Lifetime & Release-Hardening Pass ✅

- [x] **6.4.1 Scheduler Terminal Completion Guarantee**:
  - Guaranteed `OnComplete` runs exactly once across all completion and cancellation paths (queued-cancel, running-cancel, completed).
  - Ensured external resource and lease cleanup is 100% leak-proof across all job lifecycles.
- [x] **6.4.2 Chunk & Neighbor Lifetime Safety & Immutability**:
  - Implemented worker leases for all cardinal neighbors during meshing (`AcquireWorkerLease` on all ready neighbors).
  - Enforced snapshot invariant: Only `Ready` neighbors are readable; `Generating`, `Unready`, or `Unloaded` neighbors are treated as air.
  - Unloading a neighbor chunk while a worker is meshing preserves its slot memory without recycling until all active worker leases drop to 0.
- [x] **6.4.3 Component Pool Stale-Result Protection**:
  - Guarded `FinalizeChunkMesh` against stale completions from unloaded chunks. Stale mesh results are safely discarded without touching reassigned pooled components.
- [x] **6.4.4 World Shutdown Barrier**:
  - Implemented `FVoxelScheduler::WaitForAllTasks` in `UVoxelWorldSubsystem::Deinitialize`.
  - Shutdown sequence: Stop new work $\rightarrow$ flag cancellation $\rightarrow$ wait for active tasks $\rightarrow$ drain queue and release leases $\rightarrow$ destroy components $\rightarrow$ safely reset storage.
  - If timeout occurs, logs fatal development error and preserves storage to prevent memory corruption.
- [x] **6.4.5 Bounded JobStates History**:
  - Configured bounded historical retention (`MaxRetainedCompletedJobStates = 8192`) in `FVoxelScheduler` to eliminate memory growth over long-running play sessions. Active/Queued/Running jobs are never evicted.
- [x] **6.4.6 Queue Telemetry Correctness & Reset**:
  - Distinguishes Dequeued Item Latency (Avg, P50, P95, P99, Max) from Oldest Pending Queue Item Age (via queue peek).
  - `ResetDiagnosticStats()` cleanly clears subsystem latency windows.
- [x] **6.4.7 Automation Tests (28/28 Passing)**:
  - `Voxel.Streaming.SchedulerTerminalCompletion` (queued cancel, duplicate cancel, post-completion cancel)
  - `Voxel.Streaming.NeighborLifetimeSafety` (neighbor lease retention during unload and delayed recycling)
  - `Voxel.Streaming.SchedulerBoundedHistory` (2,000-job historical bounded retention)
  - `Voxel.Streaming.LongRunStress` (1,000-iteration rapid boundary crossing, churn, and slot stability)

## Just completed: Phase 7 — VoxelPhysics V1 (Real Terrain Collision) ✅

- [x] **7.1 Dedicated Physical Component Architecture (ADR-006)**:
  - Implemented `UVoxelCollisionComponent` (`UPrimitiveComponent` + `IInterface_CollisionDataProvider`).
  - Separated visual rendering (`UVoxelMeshComponent`, up to `RenderDistance=14`) from physical collision (`UVoxelCollisionComponent`, up to `SimulationDistance=4`).
- [x] **7.2 Immutable Collision Snapshot Model**:
  - `FVoxelCollisionData` generated on worker threads as plain CPU vertex/index buffers.
  - Zero UObject pointers, zero Chaos resources, and zero raw chunk pointers in collision snapshot.
  - `GetPhysicsTriMeshData` reads purely from the snapshot during Chaos BVH cooking.
- [x] **7.3 Worker-Side Greedy Collision Builder (`FVoxelCollisionBuilder`)**:
  - Binary greedy merging on collidable voxel faces, reducing triangle count by up to 90% on flat surfaces.
  - Neighbor-aware boundary face culling with active worker leases on all `Ready` cardinal neighbors.
  - Respects `UVoxelBlockDefinition::bGeneratesCollision` (filters foliage/decorative non-collidable blocks).
  - Fast-path for empty / all-air chunks (skips allocation, cooking, and physics state registration).
- [x] **7.4 Unreal Engine 5.7 Chaos Async Cooking & Lifecycle**:
  - `UBodySetup::CreatePhysicsMeshesAsync` cooks Chaos triangle mesh collision off the Game Thread.
  - Revision/Stale-result protection (`CollisionRevision` counter) rejects obsolete cook completions when chunks are modified or unloaded.
  - `RecreatePhysicsState()` registers collision with Chaos `FPhysScene`.
  - Clean teardown and `AbortPhysicsMeshAsyncCreation()` on chunk unload / world shutdown.
- [x] **7.5 Streaming Integration**:
  - Managed by `UVoxelStreamingManager` within `SimulationDistance` band.
  - Distant chunks beyond `SimulationDistance` release collision components while retaining visual rendering.
- [x] **7.6 Automation Test Suite (44/44 Passing — Exit Code: 0)**:
  - `Voxel.Physics.Cave`
  - `Voxel.Physics.CookFailure`
  - `Voxel.Physics.Deterministic`
  - `Voxel.Physics.EmptyChunk`
  - `Voxel.Physics.FlatSurface`
  - `Voxel.Physics.MissingNeighbor`
  - `Voxel.Physics.NeighborArrivalDuringCook`
  - `Voxel.Physics.NeighborBoundary`
  - `Voxel.Physics.NeighborUnloadDuringCook`
  - `Voxel.Physics.NonCollidableFilter`
  - `Voxel.Physics.OutwardWindingNormals`
  - `Voxel.Physics.SingleVoxel`
  - `Voxel.Physics.Slope`
  - `Voxel.Physics.StaleRevision`
  - `Voxel.Physics.UnloadDuringCook`

---

## 🔒 Low-Level Runtime Freeze & Next Product Phase

With Phase 6.4 and Phase 7 complete, the low-level runtime (`VoxelCore`, `VoxelRuntime`, `VoxelMath`, `VoxelAssets`, `VoxelStorage`, `VoxelGeneration`, `VoxelMeshing`, `VoxelRendering`, `VoxelPhysics`, `VoxelWorld`, `VoxelStreaming`, `VoxelDebug`) is **airtight and frozen**.

### Serialization Product Decision:
- `VoxelSerialization` (diff-based persistent terrain modification) is an explicit opt-in choice.
- If the game does not support freeform terrain mining/digging (e.g., Minecraft-style), terrain regenerates deterministically from seed + world parameters, and the game's SaveGame system handles player progress, bosses, camps, and quest state. Terrain block-diff serialization is deferred/skipped for v1.

- [ ] Chunk inspector, memory profiler, streaming visualizer
- [ ] Deliberately last — editor tooling complexity tends to balloon if started too early

## Optional / not on the critical path

- [ ] `VoxelNavigation` — nav mesh rebuild hooks on chunk changes. Optional.
- [ ] `VoxelNetworking` — not designed at all yet.

---

## Paused — World/Game Design checkpoint (do not implement yet)

These require a design decision before any code is written. See `Docs/ARCHITECTURE.md` §8 for the full architecture impact analysis.

- [ ] **ADR-006 (needed): where region/island data lives.** Region computation is inherently global — needs a precomputed, read-only structure built once from `(WorldSeed, IslandParameters)`, same pattern as `UVoxelBlockRegistry::PrecacheBiomeLayers` but at world scale.
- [ ] **Region generation algorithm.** Voronoi-like cells vs. macro-noise classification vs. constraint-solver hybrid — genuinely undecided.
- [ ] **Island Foundation parameters.** Width, length, height, sea level, coastline shape, roughness, mountain influence — needs a data contract (likely a new runtime data asset, not `UVoxelRuntimeSettings`).
- [ ] **Reserved/handcrafted content representation.** Data-asset placement (preferred) vs. tracked-modification.
- [ ] `RegionDefinition` data asset — not synonymous with biome, constrains eligible biomes per-region and feeds modifiers.
- [ ] Regional modifiers + configurable falloff/blending.
- [ ] Chapter-based progression's world-side needs (reservable landmark/boss-arena/hidden-area slots).

## Generation passes not yet implemented

- [ ] `RiverPass`, `OrePass`, `StructurePass`, `VegetationPass` — genuinely absent from `FVoxelGenerationPipeline`, not stubbed. `UVoxelBiomeDefinition::VegetationDensity` already exists as an unused field waiting for `VegetationPass`.

## Known gaps / honesty log

- [ ] `FVoxelChunkStore` is Game-Thread-only — needs a thread-safety pass once `VoxelStreaming` dispatches chunk creation from worker threads
- [ ] `TerrainPass`'s fallback block IDs are hardcoded placeholders, fine for now
- [ ] `Voxel.Generation.Cave.AirRatioLogged`, `Voxel.Generation.PerfLog`, and `Voxel.Meshing.PerfLog` intentionally have no hard pass/fail threshold — don't tighten without a profiled on-device baseline
- [ ] No pass currently checks `EVoxelJobState::Cancelled` mid-execution
- [ ] No cross-chunk mesh stitching (see VoxelMeshing section above)
- [ ] No vertex deduplication in meshing output

## Performance validation not yet done

- [ ] Profile generation AND meshing on an actual mid-range Android device (currently only measured in-editor, Development build, Windows)
- [ ] Measure memory footprint of a fully-loaded streaming radius
- [ ] Real triangle-count budget for mobile GPU, informed by the real numbers now available (e.g. one 32³ terrain chunk ≈ 2120 vertices / 1060 triangles / 3 sections, ~1.1ms to mesh) — a real per-frame budget can be derived once multiple chunks are loaded simultaneously and measured together
