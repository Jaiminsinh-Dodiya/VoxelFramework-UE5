# Voxel Framework — Architecture Decision Records (Phase 0)

Frozen decisions. Do not revisit without profiling data showing a specific
one is wrong — re-litigating these mid-implementation is the #1 way a
solo dev burns months on a framework instead of a game.

---

## ADR-001: UWorldSubsystem owns chunks, not AActor

**Decision:** Chunks are plain C++ objects (`FVoxelChunk`) owned by a
`TMap` inside a `UVoxelWorldSubsystem`. No `AActor` exists per chunk.

**Why:** Actors carry UObject overhead (reflection, GC tracking, replication
scaffolding, component hierarchy) that is pure waste for something that
exists purely as a data container + mesh reference. A subsystem is
automatically instanced once per world, has a defined lifetime tied to
world lifetime, and gives us a single well-known place for every other
module to ask "where do I find chunk X."

**Rejected alternative:** Actor-per-chunk (the V2 prototype). Rejected
because it was the actual bottleneck source in the prototype — Tick
overhead, GC pressure from thousands of spawned actors, and PMC's own
per-section overhead compound on mobile.

**Revisit only if:** profiling shows subsystem/TMap chunk lookup itself
is a bottleneck (unlikely below tens of thousands of loaded chunks).

---

## ADR-002: UE::Tasks instead of a custom thread pool

**Decision:** All generation/meshing/serialization work is submitted
through `UE::Tasks::Launch` (or `FTaskGraphInterface` for finer priority
control), not a hand-rolled `FVoxelThreadPool`.

**Why:** The engine already solves priority scheduling, worker sizing per
platform, and integration with the Render/RHI thread handoff. A custom
pool duplicates that work and has to be independently tuned per platform
(mobile core counts vary far more than desktop). Every engine update to
the task system is inherited for free instead of needing to be re-ported.

**Rejected alternative:** The `FVoxelThreadPool` built in the first pass
of this conversation. Kept as a design reference but not shipped —
replaced by task graph wrappers in VoxelCore.

**Revisit only if:** a specific platform's task graph scheduling proves
unsuitable for sustained background generation load (would need concrete
profiling evidence, not intuition).

---

## ADR-003: FVoxelChunk is a plain C++ type, not a UObject

**Decision:** `FVoxelChunk` (storage) is plain C++, heap-allocated from a
pool, referenced by `FVoxelChunkHandle` (coordinate + generation counter),
never by raw pointer across module boundaries.

**Why:** Thousands of chunks streaming in/out per play session means GC
overhead from UObject chunks would be constant and unpredictable — exactly
the "allocation spikes" the original spec explicitly rules out. Handles
with a generation counter give us stale-reference safety without needing
UObject's GC to provide it.

**Revisit only if:** editor tooling needs chunks to be UObjects for
built-in reflection/serialization convenience — if so, wrap with a thin
UObject proxy for editor-only display, don't change the runtime type.

---

## ADR-004: Rendering is separated from meshing

**Decision:** `VoxelMeshing` produces plain arrays (vertices, indices,
normals, UVs, material IDs) and knows nothing about GPU buffers or scene
proxies. `VoxelRendering` consumes that mesh data and knows nothing about
voxels, biomes, or generation.

**Why:** These are genuinely different lifetimes and different threading
rules — meshing is pure CPU worker-thread work and should be independently
unit-testable; rendering must respect render-thread command ordering and
GPU resource lifetime. Merging them was the mistake in the original single-module
prototype (mesh build and PMC upload were one function).

**Revisit only if:** never — this separation has no real downside, only
setup cost.

---

## ADR-005: Serialization is diff-based, never whole-world

**Decision:** Only modified voxels, placed/destroyed blocks, and metadata
are serialized. Untouched chunks regenerate deterministically from the
world seed on load.

**Why:** Mobile storage budgets make whole-world saves a non-starter past
a fairly small world size. Determinism from seed is "free" storage
compression — we already need deterministic generation for this to work,
so there's no reason to also pay disk cost for regenerable data.

**Revisit only if:** a gameplay requirement demands non-deterministic
per-chunk state that can't be expressed as a diff (would need a concrete
example before considering this).

## ADR-006: Dedicated UVoxelCollisionComponent & Plain CPU Collision Snapshot

**Decision:** Visual geometry (`UVoxelMeshComponent` / `VoxelRendering`) and physical
collision geometry (`UVoxelCollisionComponent` / `VoxelPhysics`) are strictly separated.
Worker threads build an immutable `FVoxelCollisionData` snapshot (vertices, indices, bounds)
which is handed to `UVoxelCollisionComponent` to cook with Chaos physics via
`IInterface_CollisionDataProvider` and `UBodySetup::CreatePhysicsMeshesAsync`.

Key design rules:
1. **Plain CPU Snapshot:** `FVoxelCollisionData` contains zero UObject pointers, zero Chaos
   objects, and zero raw chunk pointers. It is completely safe across worker threads.
2. **`bDeformableMesh = false`:** Static voxel terrain sets `bDeformableMesh = false` to enable
   Chaos static BVH optimization and vertex cleaning, avoiding overhead intended only for
   dynamically skinned skeletal meshes.
3. **Collision Revision Tracking:** Every collision request increments a monotonic `CollisionRevision`.
   Stale async cook completions (e.g. from unloaded or superseded chunks) are safely discarded.
4. **Abort Semantics:** When a chunk is unloaded or reset, `AbortPhysicsMeshAsyncCreation()`
   cancels in-flight Chaos cooks immediately, and pending cook pointers are cleared.
5. **Authoritative State Transitions:** Collision moves deterministically through:
   `NotRequired` → `Queued` → `Building` → `Cooking` → `Ready` (or `Failed`) → `Unloading`.
6. **Lease Preservation:** Neighbor chunks required for boundary culling are protected by
   worker read leases until the collision build job completes or cancels.

**Why:** Lifecycles, relevance distances, and execution stages are fundamentally different.
At `RenderDistance=14`, thousands of chunks are visible on screen, but characters only
require physical terrain collision in the immediate vicinity (`SimulationDistance=4`).
Creating and cooking Chaos `UBodySetup` for thousands of distant visual chunks would destroy
mobile frame pacing and memory budgets. Furthermore, dedicated headless servers can run
`VoxelPhysics` without loading `VoxelRendering` or GPU scene proxies at all.

**Revisit only if:** Unreal Engine deprecates `IInterface_CollisionDataProvider` or introduces
a unified hardware ray-tracing / collision proxy that renders and collides natively on GPU.

---

## ADR-007: Data-Driven Configuration Precedence and Worker-Safe Runtime Structs

**Decision:** World configuration uses a strict 4-tier precedence model:
`Project Settings (UVoxelRuntimeSettings)` → `World Definition (UVoxelWorldDefinition)` → `Presets (UVoxelStreamingPreset, UVoxelPhysicsPreset)` → `Runtime Blueprint Overrides`.

All designer-facing UDataAssets are converted into plain, immutable C++ structs (`FVoxelGenerationConfig`, `FVoxelClimateConfig`, `FVoxelTerrainConfig`, `FVoxelCaveConfig`) at initialization on the Game Thread before being passed to worker threads. Worker threads NEVER access mutable UObjects or DataAssets directly.

Key design rules:
1. **Composition over Monolithic Assets:** `UVoxelWorldDefinition` is a thin composition asset that references specialized sub-assets (`UVoxelGenerationDefinition`, `UVoxelStreamingPreset`, etc.) rather than containing hundreds of flattened properties.
2. **Domain-Specific Preset Ownership:** Presets live in their respective modules (`UVoxelStreamingPreset` in `VoxelAssets`, `UVoxelPhysicsPreset` in `VoxelPhysics`).
3. **Explicit Blueprint Query Semantics:** Query functions (e.g. `TryGetBlockAtWorldPosition`, `TryIsSolidAtWorldPosition`) use explicit residency semantics (`bool Try...`) and NEVER silently trigger synchronous generation or chunk loading from pure nodes.
4. **Validation without Policy Assumptions:** `UVoxelConfigValidator` verifies explicit errors and actionable warnings without forcing artificial constraints on independent distance bands.

**Why:** Decouples authoring and designer workflows from runtime threading constraints while guaranteeing thread safety and eliminating race conditions.

---

## Performance budgets (design constraints, not aspirations)

| System | Budget |
|---|---|
| Streaming (Game Thread decision-making) | ≤ 1.5 ms/frame |
| Generation | ≤ 2 ms/frame equivalent, budgeted across worker tasks |
| Meshing | Worker threads only, 0 ms Game Thread |
| Collision Generation | Worker threads only, 0 ms Game Thread |
| Physics Submission & Cooking | Async Chaos cooking; GT registration ≤ 0.5 ms/frame |
| Rendering submission | ≤ 1 ms/frame Game Thread |
| Serialization | Must never block Game Thread |

Any new feature gets checked against this table before it's added, not after.

