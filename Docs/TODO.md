# TODO / Future Scope

Living document. Ordered roughly by priority/dependency, not by difficulty — some "easy" items are listed late because nothing needs them yet.

See [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) §9 for the current honest list of what's deliberately not built, and [`Docs/ADR.md`](Docs/ADR.md) for decisions already frozen that future work must respect.

> **World/Game Design checkpoint still open for Region/Island systems** (see `Docs/ARCHITECTURE.md` §8). Nothing below "Paused" should be started until those are decided. `VoxelMeshing` is now complete and was unaffected by the checkpoint, exactly as predicted.

---

## Just completed: VoxelMeshing ✅

- [x] `FVoxelMeshData` — plain arrays: positions, normals, UVs, vertex colors, material IDs, indices. No engine mesh types (ADR-004 respected).
- [x] Greedy meshing algorithm — binary sweep, merges coplanar same-material faces
- [x] Hidden face removal — never emits a face between two solid voxels
- [x] Baked ambient occlusion — per-vertex corner AO, independent of merge size
- [x] Runs on worker threads via `VoxelMeshingService` → `FVoxelScheduler` (no new scheduling abstraction)
- [x] 7 automation tests: empty chunk, single voxel, adjacent-voxel merge, material boundaries, AO correctness (values match hand-derived math exactly), determinism, perf logging
- [x] **Visual validation** — extended `VoxelDebug` with a second preview mode (`GenerateAndVisualizeMeshed`) that builds a real `UProceduralMeshComponent` from `FVoxelMesher` output. Confirmed: large flat merged quads visible (proves greedy merging isn't just passing tests, it's visibly reducing geometry), no obvious winding/normal defects, no visible chunk-seam cracks in the tested view.
- [ ] **Known gap, deliberately deferred**: no cross-chunk face stitching — chunk edges always emit outward faces treating the boundary as air. Will need addressing once multiple chunks render adjacently in the real game (`VoxelWorldSubsystem`/`VoxelStreaming` territory).
- [ ] **Known gap, deliberately deferred**: no vertex deduplication across quads — correct but not maximally memory-efficient. Future optimization, not urgent.
- [ ] **Debug-tool note (not a VoxelMeshing defect)**: the `VoxelDebug` vertex-color test material produced an unexpected glowing/emissive look rather than subtle grayscale AO shading — almost certainly a material node wired to Emissive instead of Base Color in the throwaway debug material, not a bug in the baked AO data itself (the automation test's exact numeric match, 0.750/1.000, already confirms the math is correct). Worth a real, correct material once `VoxelRendering` exists; not worth chasing further in the debug tool.

## Next up: VoxelRendering

Replaces the debug tool's `UProceduralMeshComponent` (explicitly a debug-only exception to ADR-004) with the real production path.

- [ ] Custom `UMeshComponent` + `FPrimitiveSceneProxy` — **not** `ProceduralMeshComponent`, per ADR-004 and the original spec
- [ ] `FStaticMeshVertexBuffers`/`FLocalVertexFactory`, uploaded via `ENQUEUE_RENDER_COMMAND`
- [ ] Frustum culling, LOD (distance-based mesh swap)
- [ ] Texture atlas + single material (avoid per-block material switches — mobile draw call cost)
- [ ] Async mesh upload, no Game Thread stall on chunk pop-in
- [ ] Real, correct material setup for baked vertex-color AO (see debug-tool note above)

## Then: VoxelWorldSubsystem (ties generation + storage + rendering together)

This is the piece that currently doesn't exist and that `VoxelDebug` fakes by hand:

- [ ] `UWorldSubsystem` owning the "real" `FVoxelChunkStore` instance for a world
- [ ] Calls `UVoxelBlockRegistry::PrecacheBiomeLayers` once at world init, not per-caller
- [ ] Dispatches chunk generation AND meshing through `FVoxelScheduler` instead of synchronous calls
- [ ] Reads `UVoxelRuntimeSettings` for chunk size / distances instead of every caller hardcoding them
- [ ] **This is also where cross-chunk mesh stitching needs to be solved** — chunks need to know about their neighbors' edge data to avoid the seam gap noted above

## Then: VoxelStreaming

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
