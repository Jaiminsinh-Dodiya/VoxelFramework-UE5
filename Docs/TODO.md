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

## Next up: VoxelStreaming

- [ ] Priority queue keyed by distance-to-player (`FVoxelChunkCoordinate::ChebyshevDistanceTo`)
- [ ] Four independently configurable distance bands (already modeled in `UVoxelRuntimeSettings`: simulation/render/generation/persistence — just not consumed yet)
- [ ] Job chain per chunk: Generate → Mesh → Collision → GPU Upload → Finalize
- [ ] **Cancellation**: `EVoxelJobState::Cancelled` and `FVoxelScheduler::RequestCancel` already exist (designed in during Phase 1) — this is where they actually get used. Long-running pass loops need a cancellation check inserted; currently none has one.
- [ ] Streaming budget enforcement (`StreamingBudgetMs` in settings — currently unread by any code)

## Then: VoxelSerialization

- [ ] Diff-based save/load using `FVoxelChunk::GetModifications()` / `ApplyModifications()` — the data model already exists (`ADR-005`), the save format and file I/O don't
- [ ] Never serialize untouched chunks — regenerate from seed on load
- [ ] Must never block the Game Thread

## Then: VoxelEditor

- [ ] World/noise/biome preview tools (beyond what `VoxelDebug` already does)
- [ ] Chunk inspector, memory profiler, streaming visualizer
- [ ] Deliberately last — editor tooling complexity tends to balloon if started too early

## Optional / not on the critical path

- [ ] `VoxelPhysics` — Chaos collision generation from mesh data. Optional plugin dependency.
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
