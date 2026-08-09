# TODO / Future Scope

Living document. Ordered roughly by priority/dependency, not by difficulty — some "easy" items are listed late because nothing needs them yet.

See `Docs/ARCHITECTURE.md` §8 for the current honest list of what's deliberately not built, and `Docs/ADR.md` for decisions already frozen that future work must respect.

---

# Core Voxel Pipeline

## Next up: VoxelMeshing

The immediate next module. Converts a finished `FVoxelChunk` into plain vertex/index arrays — no GPU upload, no `ProceduralMeshComponent`, per `ADR-004`.

* [ ] `FVoxelMeshData` — plain arrays: positions, normals, UVs, vertex colors, material IDs, indices. No engine mesh types.
* [ ] Greedy meshing algorithm: merge coplanar same-material faces into single quads, per-axis sweep
* [ ] Hidden face removal — don't emit a face between two solid voxels
* [ ] Ambient occlusion baked into vertex colors — cheap, avoids real-time AO on mobile
* [ ] Must run entirely on worker threads, dispatched via `FVoxelScheduler`
* [ ] Automation tests: known input chunk → expected triangle count
* [ ] Automation tests: verify face merging actually reduces count vs. naive cube-per-voxel
* [ ] Perf log using the same pattern as `Voxel.Generation.PerfLog` before any hard budget is set

---

## After that: VoxelRendering

* [ ] Custom `UMeshComponent` + `FPrimitiveSceneProxy` — **not** `ProceduralMeshComponent`, per the original spec and `ADR-004`
* [ ] `FStaticMeshVertexBuffers` / `FLocalVertexFactory`
* [ ] Upload mesh data through `ENQUEUE_RENDER_COMMAND`
* [ ] Frustum culling
* [ ] LOD — distance-based mesh swap, not covered by `VoxelMeshing` itself
* [ ] Texture atlas + single material
* [ ] Avoid per-block material switches due to mobile draw-call cost
* [ ] Async mesh upload
* [ ] Ensure chunk pop-in never stalls the Game Thread

---

## Then: VoxelWorldSubsystem

This is the piece that currently doesn't exist and that `VoxelDebug` fakes by hand.

* [ ] `UWorldSubsystem` owning the real `FVoxelChunkStore` instance for a world
* [ ] Call `UVoxelBlockRegistry::PrecacheBiomeLayers` once at world init, not per caller
* [ ] Dispatch chunk generation through `FVoxelScheduler` instead of synchronous calls
* [ ] Read `UVoxelRuntimeSettings` for chunk size / distances instead of every caller hardcoding them
* [ ] Establish the authoritative lifecycle of generated chunks
* [ ] Define ownership between world, chunk store, scheduler, meshing and rendering systems
* [ ] Ensure subsystem shutdown safely cancels outstanding work

---

## Then: VoxelStreaming

* [ ] Priority queue keyed by distance-to-player using `FVoxelChunkCoordinate::ChebyshevDistanceTo`
* [ ] Four independently configurable distance bands already modeled in `UVoxelRuntimeSettings`:

  * simulation
  * rendering
  * generation
  * persistence
* [ ] Job chain per chunk:

  * Generate
  * Mesh
  * Collision
  * GPU Upload
  * Finalize
* [ ] Cancellation: `EVoxelJobState::Cancelled`
* [ ] Implement `FVoxelScheduler::RequestCancel`
* [ ] Insert cancellation checks into long-running generation pass loops
* [ ] Streaming budget enforcement using `StreamingBudgetMs`
* [ ] Prevent redundant generation requests for the same chunk
* [ ] Prevent duplicate mesh jobs for a chunk
* [ ] Define chunk lifecycle states
* [ ] Define behavior when a chunk leaves the streaming radius while work is in progress
* [ ] Ensure stale generation results cannot overwrite newer chunk state
* [ ] Test rapid player movement across many chunk boundaries

---

## Then: VoxelSerialization

* [ ] Diff-based save/load using `FVoxelChunk::GetModifications()` / `ApplyModifications()`
* [ ] Define persistent save format
* [ ] Implement file I/O
* [ ] Never serialize untouched chunks — regenerate from seed on load
* [ ] Must never block the Game Thread
* [ ] Version serialized data
* [ ] Add save-format migration strategy
* [ ] Validate corrupted/incomplete chunk data
* [ ] Test loading modified chunks over regenerated base terrain
* [ ] Test deterministic regeneration before modification replay

---

# World Foundation

## Island Definition / Foundation

The framework must support a finite, large-scale island rather than an infinite Minecraft-style world.

The island foundation is generated from high-level parameters and a deterministic world seed. Specific geography must not be hardcoded into the generator.

* [ ] `FVoxelWorldDefinition` — high-level definition of an entire generated world/island
* [ ] `FVoxelIslandDefinition` — island-specific parameters separated from runtime/streaming settings
* [ ] Define island size independently on X/Y/Z
* [ ] Define target minimum terrain elevation
* [ ] Define target maximum terrain elevation
* [ ] Define sea level
* [ ] Define coastline roughness
* [ ] Define coastline erosion/influence strength
* [ ] Define island edge falloff
* [ ] Define global terrain roughness
* [ ] Define mountain density
* [ ] Define mountain height influence
* [ ] Define valley/basin influence
* [ ] Define ocean depth profile
* [ ] Define large-scale terrain frequency independently from local terrain noise
* [ ] Define macro terrain scale
* [ ] Define local terrain scale
* [ ] Generate a deterministic island outline from the world seed and foundation parameters
* [ ] Ensure the generated island remains inside a finite world boundary
* [ ] Ensure terrain smoothly transitions into ocean around the island boundary
* [ ] Prevent disconnected floating terrain unless explicitly requested
* [ ] Support multiple island outline profiles
* [ ] Support beach-heavy coastlines
* [ ] Support mountain-to-ocean coastlines
* [ ] Support cliff-heavy coastlines
* [ ] Support flatter coastal regions
* [ ] Add deterministic world-generation tests:

  * same seed + same definition → same island
  * different seed → different valid island
* [ ] Add validation for invalid or contradictory island parameters
* [ ] Separate world-generation parameters from runtime streaming parameters
* [ ] No game-specific regions, story locations, bosses, NPCs or quest logic inside the core island generator

---

# Voxel Resolution

The framework must not assume Minecraft-style `1m × 1m × 1m` voxels.

A voxel is an internal terrain representation and its world-space size must remain configurable.

* [ ] Define voxel resolution independently from world-space meter scale
* [ ] Define world-space size of one voxel through configuration
* [ ] Ensure generation algorithms operate correctly at different voxel resolutions
* [ ] Ensure chunk dimensions are expressed in voxel counts rather than assumed world meters
* [ ] Ensure mesh generation correctly converts voxel coordinates to world-space
* [ ] Validate terrain quality at higher voxel resolutions
* [ ] Validate memory/performance impact of increased voxel density
* [ ] Ensure gameplay systems do not depend on voxel resolution
* [ ] Document recommended voxel resolutions for mobile
* [ ] Document recommended voxel resolutions for future PC targets
* [ ] Never expose the internal voxel concept as a "Minecraft block" in gameplay terminology
* [ ] Ensure voxel resolution can be changed without rewriting generation logic

---

# Region System

Regions are large logical areas of the island.

A region is not simply a biome. It represents the intended identity of an area and provides generation context consumed by terrain, biome, modifier, environment, landmark and gameplay layers.

Example:

* Ancient Forest
* Cursed Forest
* War-Torn Forest
* Mountain Kingdom
* Ancient Battlefield
* Forgotten Capital

Multiple regions may use the same biome while having completely different visual and gameplay identities.

* [ ] `FVoxelRegionDefinition` — data-driven definition of a generated region
* [ ] `FVoxelRegionInstance` — runtime/generated region information
* [ ] Define region boundaries independently from voxel chunks
* [ ] Support irregular region boundaries
* [ ] Support polygon/spline-based region boundaries
* [ ] Support multiple regions within one island
* [ ] Support deterministic region placement from world seed
* [ ] Support manually constrained region placement
* [ ] Support region priority/influence
* [ ] Support region blending at boundaries
* [ ] Support multiple generation influences overlapping the same location
* [ ] Define region metadata independently from visual biome metadata
* [ ] Allow a region to reference one or more biome definitions
* [ ] Allow multiple regions to use the same biome
* [ ] Prevent region boundaries from producing visible hard seams
* [ ] Add region visualization/debug mode
* [ ] Add region query API:

  * world position → region
  * chunk coordinate → affected regions
* [ ] Add automation tests for deterministic region assignment
* [ ] Add automation tests for region boundary blending
* [ ] Define behavior when a chunk intersects multiple regions

---

# Regional Modifier / Influence System

Generation modifiers must be spatial influences rather than hardcoded rectangular edits.

A modifier can affect terrain, materials, climate, vegetation, structures or other generation inputs.

Modifiers must support smooth falloff so customized areas can blend naturally into surrounding terrain.

* [ ] `FVoxelGenerationModifier` base interface/type
* [ ] Define modifier influence shape
* [ ] Support circular influence
* [ ] Support elliptical influence
* [ ] Support spline-based influence
* [ ] Support polygon-based influence
* [ ] Support distance-based falloff
* [ ] Support configurable falloff curves
* [ ] Support hard boundaries when explicitly required
* [ ] Support smooth blending between overlapping modifiers
* [ ] Define modifier priority/order
* [ ] Define additive influence mode
* [ ] Define multiplicative influence mode
* [ ] Define override influence mode
* [ ] Support modifier masks
* [ ] Support modifier strength
* [ ] Support modifier-local seed
* [ ] Support deterministic modifier evaluation
* [ ] Allow modifiers to affect terrain height
* [ ] Allow modifiers to affect terrain material/block selection
* [ ] Allow modifiers to affect vegetation density
* [ ] Allow modifiers to affect environmental parameters
* [ ] Allow modifiers to expose custom generation channels
* [ ] Add debug visualization for modifier influence/falloff
* [ ] Add tests for modifier blending at boundaries
* [ ] Add tests for overlapping modifiers
* [ ] Add tests for modifier priority
* [ ] Ensure modifier evaluation is thread-safe
* [ ] Ensure modifier system contains no game-specific assumptions

---

# Hierarchical Generation Pipeline

Generation must work from large-scale geography toward smaller-scale detail.

Target conceptual order:

```text
Island Foundation
        ↓
Regions
        ↓
Regional Influences / Modifiers
        ↓
Climate
        ↓
Biome
        ↓
Terrain
        ↓
Water
        ↓
Caves
        ↓
Resources
        ↓
Vegetation
        ↓
Structures
        ↓
Finalization
```

* [ ] Document authoritative ordering of all generation stages
* [ ] Define input/output contract for every generation stage
* [ ] Define which stages may modify terrain height
* [ ] Define which stages may modify voxel materials
* [ ] Define which stages may place content
* [ ] Define dependency graph between generation passes
* [ ] Prevent later passes from invalidating assumptions made by earlier passes
* [ ] Add generation context containing:

  * world seed
  * world definition
  * region data
  * modifier data
  * chunk information
  * generation settings
* [ ] Ensure every pass receives only the data it actually needs
* [ ] Ensure all generation passes remain deterministic
* [ ] Add pipeline debug output showing which layers affected a voxel/chunk
* [ ] Add per-pass timing information
* [ ] Add per-pass memory/performance diagnostics

---

# Generation Passes Not Yet Implemented

`FVoxelGenerationPipeline` currently runs:

`Climate → Biome → Terrain → Cave`

and stops.

These were in the original architecture but are genuinely absent, not stubbed.

## RiverPass

* [ ] `RiverPass`
* [ ] Carve/place water following a flow-field or noise-ridge approach
* [ ] Run after `TerrainPass`
* [ ] Determine final ordering relative to `CavePass`
* [ ] Ensure rivers can connect to oceans/lakes
* [ ] Support river width variation
* [ ] Support river depth variation
* [ ] Support river-bank terrain blending
* [ ] Prevent impossible uphill river paths
* [ ] Ensure deterministic river generation

## OrePass

* [ ] `OrePass`
* [ ] Density-based ore placement
* [ ] Similar generation model to `CavePass`
* [ ] Place blocks rather than carve terrain
* [ ] Support depth-dependent ore distribution
* [ ] Support biome/region-specific resources
* [ ] Support configurable rarity
* [ ] Ensure deterministic placement

## StructurePass

* [ ] `StructurePass`
* [ ] Handcrafted region reservation
* [ ] Support cities
* [ ] Support villages
* [ ] Support dungeons
* [ ] Support temples
* [ ] Support towers
* [ ] Support ruins
* [ ] Support spawn points
* [ ] Support boss arenas
* [ ] Introduce reserved-region concept
* [ ] Ensure procedural generation blends around reserved regions
* [ ] Prevent terrain generation from overwriting reserved content
* [ ] Define structure placement constraints
* [ ] Define structure exclusion zones

## VegetationPass

* [ ] `VegetationPass`
* [ ] Consume `UVoxelBiomeDefinition::VegetationDensity`
* [ ] Support biome-specific vegetation
* [ ] Support region-specific vegetation
* [ ] Support altitude restrictions
* [ ] Support slope restrictions
* [ ] Support water-distance restrictions
* [ ] Support deterministic vegetation placement
* [ ] Avoid vegetation placement inside reservations
* [ ] Avoid vegetation placement in unsuitable terrain

---

# World Reservation / Handcrafted Content Integration

Procedural generation must be able to reserve areas for handcrafted game content.

The generator must know that a specific area belongs to a handcrafted asset and generate terrain **around** that content rather than randomly generating through it.

* [ ] `FVoxelWorldReservation`
* [ ] Define reserved volume/shape
* [ ] Reserve terrain space for landmarks
* [ ] Reserve space for caves/dungeons
* [ ] Reserve space for cities/villages
* [ ] Reserve boss arenas
* [ ] Reserve player spawn locations
* [ ] Reserve NPC locations
* [ ] Allow generation passes to query reservations
* [ ] Prevent terrain/vegetation/structures from violating reservations
* [ ] Support terrain blending around handcrafted assets
* [ ] Support terrain flattening where required
* [ ] Support custom elevation constraints around landmarks
* [ ] Support deterministic reservation placement
* [ ] Add debug visualization for reserved areas
* [ ] Add tests verifying generation does not overwrite reserved areas
* [ ] Ensure reservations remain generic and do not reference game-specific story classes

---

# Landmark System

Large landmarks provide visual navigation, environmental storytelling and exploration targets.

A landmark is different from a normal procedural structure.

Examples:

* Giant skeleton

* Ancient temple

* Broken castle

* Massive tree

* Mountain shrine

* Colossal statue

* [ ] `FVoxelLandmarkDefinition`

* [ ] Define landmark footprint

* [ ] Define landmark height

* [ ] Define placement constraints

* [ ] Define terrain requirements

* [ ] Define minimum distance from other landmarks

* [ ] Define visibility requirements

* [ ] Define biome/region compatibility

* [ ] Define reservation requirements

* [ ] Define handcrafted asset reference without coupling the core plugin to game assets

* [ ] Support landmark placement hints

* [ ] Support landmark exclusion zones

* [ ] Support deterministic landmark placement

* [ ] Add landmark debug visualization

* [ ] Add landmark-to-region compatibility validation

* [ ] Prevent visually important landmarks from being hidden by generated terrain where visibility is required

---

# World Generation Authoring / Preview

Provide tools for designers to preview and tune an entire generated island before entering gameplay.

* [ ] Generate island preview from seed
* [ ] Change seed without restarting the editor
* [ ] Preview island heightmap
* [ ] Preview island outline
* [ ] Preview region boundaries
* [ ] Preview modifier influence
* [ ] Preview biome distribution
* [ ] Preview water
* [ ] Preview landmark locations
* [ ] Preview reserved areas
* [ ] Toggle individual generation layers
* [ ] Inspect why a specific location received a specific terrain/material
* [ ] Inspect which region affects a location
* [ ] Inspect which modifiers affect a location
* [ ] Export/import world-generation configuration
* [ ] Store reproducible world-generation presets
* [ ] Generate debug screenshots for world-generation iterations
* [ ] Add seed/parameter comparison mode

---

# Mobile Generation Constraints

The framework is mobile-first but must remain extensible for future PC targets.

* [ ] Define maximum generation work per chunk
* [ ] Define maximum mesh complexity per chunk
* [ ] Define maximum simultaneously active generation jobs
* [ ] Validate memory usage on representative Android devices
* [ ] Validate generation latency on representative Android devices
* [ ] Validate streaming behavior during rapid player movement
* [ ] Test worst-case terrain complexity
* [ ] Test worst-case modifier overlap
* [ ] Test worst-case landmark density
* [ ] Test worst-case loaded chunk count
* [ ] Ensure mobile-specific limits are configurable rather than hardcoded
* [ ] Avoid mobile-only architectural assumptions that prevent future PC builds
* [ ] Measure cold-start world generation
* [ ] Measure first-area streaming latency
* [ ] Measure sustained exploration performance
* [ ] Measure memory after prolonged exploration
* [ ] Test low-memory device behavior
* [ ] Test thermal/performance degradation during long play sessions

---

# VoxelEditor

* [ ] World/noise/biome preview tools beyond what `VoxelDebug` already does
* [ ] Island foundation preview
* [ ] Region editor/visualizer
* [ ] Modifier influence editor
* [ ] Modifier falloff visualization
* [ ] Chunk inspector
* [ ] Memory profiler
* [ ] Streaming visualizer
* [ ] Generation pass visualizer
* [ ] Reservation visualizer
* [ ] Landmark placement visualizer
* [ ] Seed/parameter comparison tools
* [ ] Deliberately keep editor tooling after core generation architecture stabilizes — editor tooling complexity tends to balloon if started too early

---

# Optional / Not on the Critical Path

These were explicitly scoped out of the default dependency graph — see `README.md`'s module graph, dashed/optional nodes.

## VoxelPhysics

* [ ] `VoxelPhysics`
* [ ] Chaos collision generation from mesh data
* [ ] Optional plugin dependency
* [ ] Collision generation must not be required by core voxel generation
* [ ] Async collision cooking where possible
* [ ] Mobile collision complexity validation

## VoxelNavigation

* [ ] `VoxelNavigation`
* [ ] Nav mesh rebuild hooks on chunk changes
* [ ] Optional dependency
* [ ] Determine whether mobile gameplay actually requires runtime navigation updates
* [ ] Avoid rebuilding navigation for purely visual terrain

## VoxelNetworking

Not designed yet.

* [ ] Define whether multiplayer is actually required
* [ ] Determine replication requirements for voxel modifications
* [ ] Determine replication cost before implementation
* [ ] Design chunk synchronization model
* [ ] Design modified-voxel replication
* [ ] Design world-seed synchronization
* [ ] Design server-authoritative generation if multiplayer is ever required

---

# Plugin Productization / Game Separation

The voxel framework must remain independently reusable outside the primary game project.

The game is the primary production/test environment for the plugin, but the plugin itself must never become dependent on the game's story, assets or gameplay systems.

* [ ] Define strict boundary between generic plugin code and game-specific code
* [ ] No game-specific class names inside core voxel modules
* [ ] No direct references to game characters
* [ ] No direct references to quests
* [ ] No direct references to bosses
* [ ] No direct references to story systems
* [ ] No direct references to game assets
* [ ] No hardcoded game world dimensions
* [ ] No hardcoded game biomes
* [ ] No hardcoded game regions
* [ ] No hardcoded game landmarks
* [ ] No hardcoded game progression
* [ ] No hardcoded game story
* [ ] No hardcoded game-specific enemy logic
* [ ] Expose extension points instead of game-specific implementations
* [ ] Define public API surface for external projects
* [ ] Define internal/private implementation modules
* [ ] Document plugin configuration workflow
* [ ] Create minimal standalone sample project using the plugin
* [ ] Create sample generated island containing no game assets
* [ ] Ensure plugin can be packaged independently
* [ ] Test plugin in a clean Unreal project
* [ ] Add plugin versioning strategy
* [ ] Add migration notes for breaking API changes
* [ ] Add plugin documentation separate from game documentation
* [ ] Define licensing strategy
* [ ] Define commercial distribution strategy
* [ ] Define Marketplace/package distribution requirements
* [ ] Define example/demo content that can ship with the plugin
* [ ] Ensure demo content is legally distributable separately from the game
* [ ] Maintain a clean separation between plugin source and game source control
* [ ] Periodically test the plugin in a clean project to detect accidental dependencies

---

# Game World Integration

This section belongs to the game project rather than the reusable voxel framework.

The game uses the voxel framework to construct a handcrafted narrative experience on top of a deterministic procedural island.

The world is procedural.

The story is handcrafted.

Procedural generation determines geography, environmental variation and exploration context; story-critical events and narrative content remain controlled by the game.

## Chapter-Based Progression

* [ ] Define chapter-based world progression
* [ ] Define which island regions belong to each chapter
* [ ] Define region unlock conditions
* [ ] Define main objective per region
* [ ] Define main boss per region
* [ ] Define optional bosses per region
* [ ] Define NPC encounter locations
* [ ] Define side quest trigger locations
* [ ] Define hidden dungeon locations
* [ ] Define legendary weapon locations
* [ ] Define major landmarks visible from long distances
* [ ] Define fast-travel/checkpoint structure
* [ ] Define how the world changes after major story events
* [ ] Define chapter transition system
* [ ] Define save-state requirements for world progression
* [ ] Define which procedural elements may vary between saves
* [ ] Define which story-critical locations must remain deterministic
* [ ] Define how procedural terrain supports handcrafted story locations

## Region Gameplay Loop

Target progression structure:

```text
Chapter
   ↓
Large Open Region
   ↓
Explore
   ↓
Main Objective
   ↓
NPC Encounters
   ↓
Side Quests
   ↓
Hidden Areas
   ↓
Optional Bosses
   ↓
Dungeons / Temples
   ↓
Legendary Rewards
   ↓
Main Boss
   ↓
Story Progression
   ↓
Next Region / Chapter
```

* [ ] Define minimum required content for each major region
* [ ] Define recommended exploration time per region
* [ ] Define main path
* [ ] Define optional paths
* [ ] Define hidden paths
* [ ] Define shortcuts
* [ ] Define optional boss progression
* [ ] Define region-specific rewards
* [ ] Define region-specific lore
* [ ] Define region completion state
* [ ] Define what causes the next region to unlock

---

# Story / Procedural Boundary

Story content must not depend on random generation in a way that makes narrative progression unreliable.

* [ ] Define story-critical locations
* [ ] Define story-critical landmarks
* [ ] Define story-critical NPC locations
* [ ] Define story-critical boss arenas
* [ ] Define story-critical item locations
* [ ] Ensure story-critical content receives world reservations
* [ ] Ensure procedural generation cannot invalidate story-critical content
* [ ] Allow surrounding terrain to vary while keeping critical locations deterministic
* [ ] Define which world elements are allowed to move between seeds
* [ ] Define which world elements must remain fixed
* [ ] Define chapter-specific world-state changes
* [ ] Define post-boss world changes
* [ ] Define permanent world changes
* [ ] Define temporary world-state changes

---

# Side Quest Integration

Side quests should be handcrafted narrative content whose possible locations can be selected from valid generated contexts.

The voxel framework must provide location/context information but must not own quest logic.

* [ ] Define side quest trigger types
* [ ] Define location requirements for each side quest
* [ ] Define environmental requirements
* [ ] Define region requirements
* [ ] Define landmark requirements
* [ ] Define NPC requirements
* [ ] Define dungeon requirements
* [ ] Define quest exclusion rules
* [ ] Define quest priority
* [ ] Define quest uniqueness
* [ ] Define whether a quest can appear more than once
* [ ] Define deterministic quest placement from world seed
* [ ] Define fallback locations when preferred locations are unavailable
* [ ] Define quest state independently from voxel generation
* [ ] Ensure quest generation cannot alter core voxel framework behavior

---

# Boss / Dungeon Integration

Boss encounters are handcrafted gameplay experiences.

Procedural terrain should support the surrounding environment without randomly changing the actual boss encounter design.

* [ ] Define boss arena reservation system
* [ ] Define arena terrain requirements
* [ ] Define boss approach route
* [ ] Define environmental storytelling around bosses
* [ ] Define optional boss placement rules
* [ ] Define main boss placement rules
* [ ] Define hidden boss placement rules
* [ ] Define boss unlock requirements
* [ ] Define post-boss world changes
* [ ] Define boss reward locations
* [ ] Define legendary weapon/relic relationships
* [ ] Ensure boss arenas remain stable across procedural seeds where required

---

# Multiple Endings

Multiple endings are a core design priority.

* [ ] Define ending count
* [ ] Define ending conditions
* [ ] Define player choices affecting endings
* [ ] Define NPC relationships affecting endings
* [ ] Define optional boss/event requirements affecting endings
* [ ] Define hidden requirements
* [ ] Define ending flags in save data
* [ ] Define when ending state is locked
* [ ] Define whether ending state can be changed before final commitment
* [ ] Ensure ending logic remains independent of procedural terrain generation
* [ ] Define New Game Plus interaction with ending progression if required

---

# World Exploration Design

The world should encourage players to look into the distance and investigate visible points of interest.

* [ ] Define visual exploration philosophy
* [ ] Define major landmark visibility rules
* [ ] Define environmental storytelling rules
* [ ] Define distance-based points of interest
* [ ] Define hidden exploration targets
* [ ] Define vertical exploration
* [ ] Define caves and underground routes
* [ ] Define shortcuts
* [ ] Define alternate routes
* [ ] Define secret paths
* [ ] Define environmental clues
* [ ] Define landmark-to-landmark navigation
* [ ] Define player guidance without excessive waypoint dependence

---

# Mobile Gameplay Structure

The game should be designed around mobile session length rather than simply shrinking a console/PC game.

* [ ] Define target session length
* [ ] Define natural stopping points
* [ ] Define checkpoint frequency
* [ ] Define save/autosave behavior
* [ ] Define loading strategy between major regions
* [ ] Define region streaming strategy
* [ ] Define boss encounter session expectations
* [ ] Define side quest session expectations
* [ ] Define exploration session expectations
* [ ] Ensure players can make meaningful progress during short sessions
* [ ] Ensure longer sessions still provide meaningful exploration
* [ ] Avoid requiring extremely long uninterrupted sessions for critical progression
* [ ] Define mobile-specific UI/UX requirements separately from world-generation systems

---

# Performance Validation

The current budget table in `README.md` is a design constraint, not a measured guarantee.

* [ ] Profile generation on an actual mid-range Android device
* [ ] Profile generation on a lower-end target Android device
* [ ] Profile generation on a high-end Android device
* [ ] Measure memory footprint of a fully loaded streaming radius
* [ ] Compare loaded memory against `MemoryBudgetMB`
* [ ] Measure chunk generation time
* [ ] Measure mesh generation time
* [ ] Measure mesh upload time
* [ ] Measure collision generation time
* [ ] Measure streaming latency
* [ ] Measure triangle count per chunk
* [ ] Measure total visible triangle count
* [ ] Measure draw calls
* [ ] Measure material switches
* [ ] Measure CPU usage during exploration
* [ ] Measure GPU usage during exploration
* [ ] Measure memory fragmentation
* [ ] Measure worst-case region complexity
* [ ] Measure worst-case modifier overlap
* [ ] Measure worst-case landmark density
* [ ] Measure performance after prolonged gameplay
* [ ] Establish real device-based budgets before enforcing hard thresholds

---

# Known Gaps / Honesty Log

Things that work but have a known limitation worth fixing before relying on them further:

* [ ] `FVoxelChunkStore` is Game-Thread-only — will need a thread-safety pass once `VoxelStreaming` dispatches chunk creation from worker threads
* [ ] `TerrainPass` fallback block IDs (`FallbackStoneId=1`, `FallbackDirtId=2`, `FallbackGrassId=3`) are hardcoded placeholders used only when no biome is assigned
* [ ] If a project ships without biomes, fallback IDs should become proper settings-driven defaults
* [ ] `Voxel.Generation.Cave.AirRatioLogged` intentionally has no hard pass/fail threshold
* [ ] `Voxel.Generation.PerfLog` intentionally has no hard pass/fail threshold
* [ ] Do not tighten generation thresholds without an actual profiled baseline
* [ ] Prefer on-device profiling rather than editor-only profiling
* [ ] No pass currently checks `EVoxelJobState::Cancelled` mid-execution
* [ ] Cancellation state exists but is not yet consumed by long-running generation work
* [ ] `FVoxelChunkStore` thread-safety must be resolved before worker-thread ownership is expanded
* [ ] Region data model does not yet exist
* [ ] Modifier influence data model does not yet exist
* [ ] Reservation data model does not yet exist
* [ ] Landmark data model does not yet exist
* [ ] Island foundation definition does not yet exist
* [ ] World-generation/game-content boundary must be maintained before game-specific generation features are added

---

# Architecture / Documentation

* [ ] Update `Docs/ARCHITECTURE.md` whenever a major generation-layer decision is frozen
* [ ] Add island-generation architecture section
* [ ] Add region architecture section
* [ ] Add modifier/influence architecture section
* [ ] Add reservation architecture section
* [ ] Add landmark architecture section
* [ ] Document generation pass ordering
* [ ] Document thread-safety expectations for every generation pass
* [ ] Document deterministic-generation requirements
* [ ] Document plugin/game separation rules
* [ ] Document public plugin API
* [ ] Document which systems are game-specific
* [ ] Record major architecture decisions in `Docs/ADR.md`
* [ ] Add ADR for finite procedural island design
* [ ] Add ADR for configurable voxel resolution
* [ ] Add ADR for region-vs-biome separation
* [ ] Add ADR for spatial generation modifiers
* [ ] Add ADR for handcrafted content reservations
* [ ] Add ADR for procedural-world / handcrafted-story separation
* [ ] Add ADR for plugin/game separation

---

# Future / Optional Expansion

These are intentionally not part of the first production target.

## PC Expansion

* [ ] Validate higher voxel resolutions
* [ ] Validate larger streaming distances
* [ ] Validate higher terrain complexity
* [ ] Validate higher landmark density
* [ ] Validate higher simulation distances
* [ ] Add PC-specific performance profiles
* [ ] Support larger worlds if justified by actual gameplay needs
* [ ] Never allow PC expansion requirements to compromise the first mobile release unnecessarily

## Micro-Voxel / Higher Resolution Landscape

Future experimental direction.

* [ ] Investigate sub-meter voxel resolution
* [ ] Investigate adaptive voxel resolution
* [ ] Investigate multi-resolution chunk representation
* [ ] Investigate terrain quality at higher resolutions
* [ ] Investigate memory implications
* [ ] Investigate meshing cost
* [ ] Investigate mobile feasibility
* [ ] Investigate PC-only high-resolution profile
* [ ] Do not implement until the core game and first production version are proven

## Advanced Terrain

* [ ] Erosion simulation
* [ ] Hydraulic erosion approximation
* [ ] Thermal erosion approximation
* [ ] Advanced river simulation
* [ ] Advanced coastline generation
* [ ] Advanced mountain formation
* [ ] Geological strata
* [ ] Advanced cave networks
* [ ] Underground biomes
* [ ] Procedural cliffs
* [ ] Advanced terrain material blending

---

# Production Principle

The voxel framework exists to support the game.

The game must not exist merely to justify the voxel framework.

Core priorities:

1. Build the game.
2. Keep the reusable framework generic.
3. Design the world before expanding generation technology.
4. Generate geography procedurally.
5. Keep story-critical content handcrafted.
6. Use regions and modifiers to create meaningful environmental history.
7. Reserve space for handcrafted landmarks and gameplay.
8. Optimize for mobile first.
9. Keep the architecture extensible for future PC/high-resolution targets.
10. Only build advanced voxel technology when the actual game requires it.
11. Validate performance on real devices before establishing hard budgets.
12. Keep the commercial plugin independently usable and distributable.

The intended long-term relationship is:

```text
                GAME
                  │
                  │ uses
                  ▼
          VOXEL FRAMEWORK
                  │
                  │ generates
                  ▼
          PROCEDURAL WORLD
                  │
                  │ receives
                  ▼
       GAME-SPECIFIC CONTENT
                  │
        ┌─────────┼─────────┐
        ▼         ▼         ▼
      Story     Bosses     Quests
        │         │         │
        └─────────┼─────────┘
                  ▼
             PLAYER
```

The framework should provide the technology.

The game should provide the identity.

The commercial plugin should remain useful even when the game's world, story, characters and assets are completely removed.
