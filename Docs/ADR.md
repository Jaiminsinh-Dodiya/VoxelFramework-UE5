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

---

## Performance budgets (design constraints, not aspirations)

| System | Budget |
|---|---|
| Streaming (Game Thread decision-making) | ≤ 1.5 ms/frame |
| Generation | ≤ 2 ms/frame equivalent, budgeted across worker tasks |
| Meshing | Worker threads only, 0 ms Game Thread |
| Rendering submission | ≤ 1 ms/frame Game Thread |
| Serialization | Must never block Game Thread |

Any new feature gets checked against this table before it's added, not after.

## Scheduler cancellation stance

Job/task state is modeled from day one as:

```cpp
enum class EVoxelJobState : uint8
{
    Queued,
    Running,
    Completed,
    Cancelled
};
```

`Cancelled` is not driven by any logic yet in Phase 1–4. The state exists
so Phase 5 streaming can add real cancellation without touching the job
data model.
