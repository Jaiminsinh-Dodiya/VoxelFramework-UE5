# TODO / Future Scope

Living document. Ordered roughly by priority/dependency, not by difficulty — some "easy" items are listed late because nothing needs them yet.

See [`Docs/ARCHITECTURE.md`](Docs/ARCHITECTURE.md) §8 for the current honest list of what's deliberately not built, and [`Docs/ADR.md`](Docs/ADR.md) for decisions already frozen that future work must respect.

---

## Next up: VoxelMeshing

The immediate next module. Converts a finished `FVoxelChunk` into plain vertex/index arrays — no GPU upload, no `ProceduralMeshComponent`, per `ADR-004`.

- [ ] `FVoxelMeshData` — plain arrays: positions, normals, UVs, vertex colors, material IDs, indices. No engine mesh types.
- [ ] Greedy meshing algorithm: merge coplanar same-material faces into single quads, per-axis sweep
- [ ] Hidden face removal (don't emit a face between two solid voxels)
- [ ] Ambient occlusion baked into vertex colors (cheap, avoids real-time AO on mobile)
- [ ] Must run entirely on worker threads, dispatched via `FVoxelScheduler`
- [ ] Automation tests: known input chunk → expected triangle count; verify face merging actually reduces count vs. naive cube-per-voxel
- [ ] Perf log (same pattern as `Voxel.Generation.PerfLog`) before any hard budget is set

## After that: VoxelRendering

- [ ] Custom `UMeshComponent` + `FPrimitiveSceneProxy` — **not** `ProceduralMeshComponent`, per the original spec and `ADR-004`
- [ ] `FStaticMeshVertexBuffers`/`FLocalVertexFactory`, uploaded via `ENQUEUE_RENDER_COMMAND`
- [ ] Frustum culling, LOD (distance-based mesh swap, not covered by VoxelMeshing itself)
- [ ] Texture atlas + single material (avoid per-block material switches — mobile draw call cost)
- [ ] Async mesh upload, no Game Thread stall on chunk pop-in

## Then: VoxelWorldSubsystem (ties generation + storage + rendering together)

This is the piece that currently doesn't exist and that `VoxelDebug` fakes by hand:

- [ ] `UWorldSubsystem` owning the "real" `FVoxelChunkStore` instance for a world
- [ ] Calls `UVoxelBlockRegistry::PrecacheBiomeLayers` once at world init, not per-caller
- [ ] Dispatches chunk generation through `FVoxelScheduler` instead of synchronous calls
- [ ] Reads `UVoxelRuntimeSettings` for chunk size / distances instead of every caller hardcoding them

## Then: VoxelStreaming

- [ ] Priority queue keyed by distance-to-player (`FVoxelChunkCoordinate::ChebyshevDistanceTo`)
- [ ] Four independently configurable distance bands (already modeled in `UVoxelRuntimeSettings`: simulation/render/generation/persistence — just not consumed yet)
- [ ] Job chain per chunk: Generate → Mesh → Collision → GPU Upload → Finalize
- [ ] **Cancellation**: `EVoxelJobState::Cancelled` and `FVoxelScheduler::RequestCancel` already exist (designed in during Phase 1, see `ADR.md`) — this is where they actually get used. Long-running pass loops need a cancellation check inserted; currently none has one.
- [ ] Streaming budget enforcement (`StreamingBudgetMs` in settings — currently unread by any code)

## Then: VoxelSerialization

- [ ] Diff-based save/load using `FVoxelChunk::GetModifications()` / `ApplyModifications()` — the data model already exists (`ADR-005`), the save format and file I/O don't
- [ ] Never serialize untouched chunks — regenerate from seed on load
- [ ] Must never block the Game Thread (per the performance budget table in `README.md`)

## Then: VoxelEditor

- [ ] World/noise/biome preview tools (beyond what `VoxelDebug` already does)
- [ ] Chunk inspector, memory profiler, streaming visualizer
- [ ] Deliberately last — editor tooling complexity tends to balloon if started too early, per the roadmap discussion that shaped this project's phasing

## Optional / not on the critical path

These were explicitly scoped out of the default dependency graph — see `README.md`'s module graph, dashed/optional nodes:

- [ ] `VoxelPhysics` — Chaos collision generation from mesh data. Optional plugin dependency, not required core.
- [ ] `VoxelNavigation` — nav mesh rebuild hooks on chunk changes. Optional; many mobile games won't need this.
- [ ] `VoxelNetworking` — not designed at all yet. Would need real thought about replication cost for voxel edits before starting.

---

## Generation passes not yet implemented

`FVoxelGenerationPipeline` currently runs `Climate → Biome → Terrain → Cave` and stops. These were in the original architecture but are genuinely absent, not stubbed:

- [ ] `RiverPass` — carve/place water following a flow-field or noise-ridge approach; must run after `TerrainPass`, likely before `CavePass` so caves don't intersect river beds unpredictably
- [ ] `OrePass` — density-based ore placement, similar shape to `CavePass` but placing blocks instead of carving
- [ ] `StructurePass` — handcrafted region reservation (cities, dungeons, spawn points) that procedural generation must blend around; needs a "reserved region" concept that doesn't exist yet anywhere in the data model
- [ ] `VegetationPass` — `UVoxelBiomeDefinition::VegetationDensity` already exists as a field and is completely unused; this is the pass that would consume it

## Known gaps / honesty log

Things that work but have a known limitation worth fixing before relying on them further:

- [ ] `FVoxelChunkStore` is Game-Thread-only — will need a thread-safety pass once `VoxelStreaming` dispatches chunk creation from worker threads
- [ ] `TerrainPass`'s fallback block IDs (`FallbackStoneId=1`, `FallbackDirtId=2`, `FallbackGrassId=3`) are hardcoded placeholders used only when no biome is assigned — fine for now, but if a project ships without biomes at all, these should probably become a proper settings-driven default instead of magic numbers in `TerrainPass.cpp`
- [ ] `Voxel.Generation.Cave.AirRatioLogged` and `Voxel.Generation.PerfLog` intentionally have no hard pass/fail threshold — see `Docs/ARCHITECTURE.md` §7 for why, and don't tighten these without an actual profiled baseline (ideally on-device, not just in-editor)
- [ ] No pass currently checks `EVoxelJobState::Cancelled` mid-execution — the state exists, nothing reads it yet during long-running work

## Performance validation not yet done

The budget table in `README.md` is a design constraint, not a measured guarantee:

- [ ] Profile generation on an actual mid-range Android device (currently only measured in-editor, Development build, Windows)
- [ ] Measure memory footprint of a fully-loaded streaming radius, compare against `MemoryBudgetMB`
- [ ] Once `VoxelMeshing` exists: triangle count and mesh generation time per chunk, compared against a real mobile GPU budget
