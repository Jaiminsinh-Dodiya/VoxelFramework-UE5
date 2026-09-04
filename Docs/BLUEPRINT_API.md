# VoxelFramework — Blueprint API Reference

> This document covers **every** Blueprint-exposed function, property, data asset, and enum in VoxelFramework.
> All functions are accessible from the **Blueprint graph** by searching their name or browsing their category.

---

## How to Access Voxel Functions in Blueprints

VoxelFramework provides **two ways** to call its functions from Blueprints:

### Method 1: Static Blueprint Libraries (Recommended — Easiest)

VoxelFramework includes **static function libraries** that work from **any** Blueprint without needing to manually get a subsystem reference. Just right-click in the graph and search:

| Search For | What You Get |
|---|---|
| `World Position to Chunk Coordinate` | Converts actor location → chunk coord |
| `Try Get Block at World Position` | Gets the block ID at a location |
| `Try Is Solid at World Position` | Checks if block is solid |
| `Is Chunk Loaded` | Checks if a chunk is generated |
| `Is Chunk Collision Ready` | Checks if collision is cooked |
| `Apply World Definition` | Applies a world config asset |
| `Set Streaming Render Distance` | Changes render distance at runtime |
| `Set Streaming Simulation Distance` | Changes physics collision distance |
| `Apply Streaming Preset` | Applies a streaming preset asset |
| `Set Streaming Frozen` | Freezes chunk loading (debug) |

These come from `UVoxelBlueprintLibrary` (world queries) and `UVoxelStreamingBlueprintLibrary` (streaming controls). The `WorldContextObject` pin is auto-filled by Unreal — you don't need to wire it.

### Method 2: World Subsystem References (Advanced)

For full access to every function, get the subsystem directly:

**VoxelWorldSubsystem:**
- Right-click in graph → search **"Get Subsystem"** → **"Get World Subsystem"** → class: **VoxelWorldSubsystem**

**VoxelStreamingManager:**
- Same process, class: **VoxelStreamingManager**

This works from any Blueprint (Actor, Character, Widget, GameMode, etc.).

---

## 0. Static Blueprint Libraries (UVoxelBlueprintLibrary & UVoxelStreamingBlueprintLibrary)

These are the **easiest** way to use VoxelFramework from Blueprints. All functions appear directly in the right-click search without needing a subsystem reference.

### UVoxelBlueprintLibrary (Category: `Voxel|World`, `Voxel|Query`, `Voxel|Chunk`)

Source: `VoxelWorld/Public/VoxelBlueprintLibrary.h`

| Node Name | Category | Inputs | Outputs | Description |
|---|---|---|---|---|
| `Get Voxel World Subsystem` | `Voxel\|World` | *(auto)* | `UVoxelWorldSubsystem*` | Returns the world subsystem instance. |
| `World Position to Chunk Coordinate` | `Voxel\|Query` | `WorldPosition` (FVector) | `FIntVector` | Converts world location to chunk coordinate. Input: `GetActorLocation`. |
| `Try Get Block at World Position` | `Voxel\|Query` | `WorldPosition` (FVector) | `bool` (success), `OutBlockId` (int) | Gets block ID. Returns false if chunk not loaded. |
| `Try Is Solid at World Position` | `Voxel\|Query` | `WorldPosition` (FVector) | `bool` (success), `bOutIsSolid` (bool) | Checks solid/air. Returns false if chunk not loaded. |
| `Is Chunk Loaded` | `Voxel\|Chunk` | `ChunkCoord` (FIntVector) | `bool` | True if chunk is generated and resident. |
| `Is Chunk Collision Ready` | `Voxel\|Chunk` | `ChunkCoord` (FIntVector) | `bool` | True if physics collision is active. |
| `Get Chunk Size` | `Voxel\|World` | *(auto)* | `int32` | Chunk edge size in voxels (e.g. 32). |
| `Get World Seed` | `Voxel\|World` | *(auto)* | `int32` | Active world generation seed. |
| `Get Voxel World Size` | `Voxel\|World` | *(auto)* | `float` | Voxel size in cm (default 100 = 1m). |
| `Is World Initialized` | `Voxel\|World` | *(auto)* | `bool` | True if voxel world is ready. |
| `Apply World Definition` | `Voxel\|World` | `WorldDefinition` (UVoxelWorldDefinition*) | *none* | Applies a world config data asset. |

### UVoxelStreamingBlueprintLibrary (Category: `Voxel|Streaming`, `Voxel|Development`)

Source: `VoxelStreaming/Public/VoxelStreamingBlueprintLibrary.h`

| Node Name | Category | Inputs | Outputs | Description |
|---|---|---|---|---|
| `Get Voxel Streaming Manager` | `Voxel\|Streaming` | *(auto)* | `UVoxelStreamingManager*` | Returns the streaming manager. |
| `Apply Streaming Preset` | `Voxel\|Streaming` | `Preset` (UVoxelStreamingPreset*) | *none* | Applies all distance bands + budget from preset. |
| `Set Streaming Render Distance` | `Voxel\|Streaming` | `NewRenderDistance` (int, chunks) | *none* | Sets visible chunk radius. |
| `Get Streaming Render Distance` | `Voxel\|Streaming` | *(auto)* | `int32` | Current render distance in chunks. |
| `Set Streaming Simulation Distance` | `Voxel\|Streaming` | `NewSimulationDistance` (int, chunks) | *none* | Sets physics collision radius. |
| `Get Streaming Simulation Distance` | `Voxel\|Streaming` | *(auto)* | `int32` | Current simulation distance in chunks. |
| `Set Streaming Budget Ms` | `Voxel\|Streaming` | `NewBudgetMs` (float) | *none* | Sets frame time budget for streaming. |
| `Get Streaming Budget Ms` | `Voxel\|Streaming` | *(auto)* | `float` | Current streaming budget in ms. |
| `Set Streaming Frozen` | `Voxel\|Development` | `bFrozen` (bool) | *none* | Freezes/unfreezes chunk loading. |
| `Is Streaming Frozen` | `Voxel\|Development` | *(auto)* | `bool` | True if streaming is frozen. |



## 1. UVoxelWorldSubsystem — Blueprint Functions

Access via: **Get World Subsystem → Voxel World Subsystem**

All functions are Game Thread only.

---

### Category: `Voxel|World`

#### ClearAllChunks
| | |
|---|---|
| **Node Name** | `Clear All Chunks` |
| **Category** | `Voxel\|World` |
| **Type** | BlueprintCallable (action node) |
| **Input** | *none* |
| **Output** | *none* |
| **Description** | Immediately unloads all generated chunks and cancels any pending generation jobs. Use this when resetting the world or before loading a new save. |

---

#### ApplyWorldDefinition
| | |
|---|---|
| **Node Name** | `Apply World Definition` |
| **Category** | `Voxel\|World` |
| **Type** | BlueprintCallable (action node) |
| **Input** | `In World Definition` — A `UVoxelWorldDefinition` data asset reference. Create one in Content Browser → Miscellaneous → Data Asset → VoxelWorldDefinition. |
| **Output** | *none* |
| **Description** | Sets the master world configuration. Applies seed, voxel scale, biomes, materials, generation config, streaming preset, and physics preset from the given data asset. Call this before requesting any chunks, or call `ClearAllChunks` first when switching worlds at runtime. |

---

#### GetChunkSize
| | |
|---|---|
| **Node Name** | `Get Chunk Size` |
| **Category** | `Voxel\|World` |
| **Type** | BlueprintPure (value node) |
| **Input** | *none* |
| **Output** | `Return Value` — `Integer` — Number of voxels along one edge of a chunk (e.g. `32`). |
| **Description** | Returns the chunk edge size in voxels. A chunk is a 3D cube of `ChunkSize × ChunkSize × ChunkSize` blocks. |

---

#### GetWorldSeed
| | |
|---|---|
| **Node Name** | `Get World Seed` |
| **Category** | `Voxel\|World` |
| **Type** | BlueprintPure (value node) |
| **Input** | *none* |
| **Output** | `Return Value` — `Integer` — The active world generation seed. |
| **Description** | Returns the random seed currently used for world generation. Two worlds with the same seed + same generation definition produce identical terrain. |

---

#### GetVoxelWorldSize
| | |
|---|---|
| **Node Name** | `Get Voxel World Size` |
| **Category** | `Voxel\|World` |
| **Type** | BlueprintPure (value node) |
| **Input** | *none* |
| **Output** | `Return Value` — `Float` — Size of one voxel in Unreal units (cm). Default: `100.0` = 1 meter. |
| **Description** | Returns the visual scale of a single voxel. Multiply by `GetChunkSize()` to get the world-space edge length of one chunk. |

---

#### IsWorldInitialized
| | |
|---|---|
| **Node Name** | `Is World Initialized` |
| **Category** | `Voxel\|World` |
| **Type** | BlueprintPure (value node) |
| **Input** | *none* |
| **Output** | `Return Value` — `Boolean` — `true` if the voxel world is ready. |
| **Description** | Returns whether the Voxel World Subsystem has been initialized and is ready to accept chunk requests. Use this to guard early Blueprint logic that depends on voxel data. |

---

### Category: `Voxel|Query`

Query functions are safe and cheap. They only read from already-loaded chunks. They will **NEVER** trigger chunk generation, loading, or frame drops.

#### TryGetBlockAtWorldPosition
| | |
|---|---|
| **Node Name** | `Try Get Block at World Position` |
| **Category** | `Voxel\|Query` |
| **Type** | BlueprintPure (value node) |
| **Inputs** | |
| `World Position` | `FVector` — A 3D location in Unreal world units (cm). Get this from `GetActorLocation`, `GetHitResult → Impact Point`, cursor traces, etc. |
| **Outputs** | |
| `Return Value` | `Boolean` — `true` if the chunk containing this position is loaded and the query succeeded. `false` if the chunk is not loaded (in which case `Out Block Id` is invalid). |
| `Out Block Id` | `Integer` — The block ID at that position (e.g. `0` = Air, `1` = Stone, `2` = Dirt, `3` = Grass). Only valid when return is `true`. |
| **Description** | Looks up which block exists at a world location. **Does NOT trigger loading.** If the chunk isn't resident yet, returns `false` — your Blueprint should handle this gracefully (e.g. retry next frame, show "loading" UI). |

**Blueprint Usage Example:**
```
GetActorLocation → TryGetBlockAtWorldPosition → Branch (Return Value)
                                                    ├─ True → Print String (OutBlockId)
                                                    └─ False → Print String ("Chunk not loaded")
```

---

#### TryIsSolidAtWorldPosition
| | |
|---|---|
| **Node Name** | `Try Is Solid at World Position` |
| **Category** | `Voxel\|Query` |
| **Type** | BlueprintPure (value node) |
| **Inputs** | |
| `World Position` | `FVector` — A 3D location in Unreal world units (cm). Get from `GetActorLocation`, etc. |
| **Outputs** | |
| `Return Value` | `Boolean` — `true` if the chunk is loaded, `false` if not loaded (in which case `bOutIsSolid` is invalid). |
| `bOut Is Solid` | `Boolean` — `true` if the block at this position is solid (not air), `false` if it is air/empty. Only valid when return is `true`. |
| **Description** | Quick solid/air check without needing to know the exact block ID. Useful for ground detection, line-of-sight checks, or buildability queries. |

---

#### WorldPositionToChunkCoordinate
| | |
|---|---|
| **Node Name** | `World Position to Chunk Coordinate` |
| **Category** | `Voxel\|Query` |
| **Type** | BlueprintPure (value node) |
| **Inputs** | |
| `World Position` | `FVector` — Any 3D location in Unreal world units (cm). Get from `GetActorLocation`, `GetPlayerPawn → GetActorLocation`, etc. |
| **Outputs** | |
| `Return Value` | `FIntVector` (Integer Vector) — The chunk coordinate (X, Y, Z in chunk-space) that contains the given world position. |
| **Description** | Converts an Unreal world position to the integer chunk coordinate that contains it. Use this to know which chunk the player is standing in, or to check if a specific chunk is loaded. |

**Blueprint Usage Example:**
```
GetActorLocation → WorldPositionToChunkCoordinate → Print String (Break IntVector → "Chunk: X, Y, Z")
```

**Tip:** To convert back: `ChunkWorldPosition = ChunkCoord * ChunkSize * VoxelWorldSize`

---

### Category: `Voxel|Chunk`

#### IsChunkLoaded
| | |
|---|---|
| **Node Name** | `Is Chunk Loaded` |
| **Category** | `Voxel\|Chunk` |
| **Type** | BlueprintPure (value node) |
| **Inputs** | |
| `Chunk Coord` | `FIntVector` (Integer Vector) — The chunk coordinate to check. Get this from `WorldPositionToChunkCoordinate`. |
| **Outputs** | |
| `Return Value` | `Boolean` — `true` if the chunk has finished generating and its mesh is resident. |
| **Description** | Checks whether a specific chunk has completed generation and is loaded. Use with `WorldPositionToChunkCoordinate` to check if the terrain around the player is ready. |

---

#### IsChunkCollisionReady
| | |
|---|---|
| **Node Name** | `Is Chunk Collision Ready` |
| **Category** | `Voxel\|Chunk` |
| **Type** | BlueprintPure (value node) |
| **Inputs** | |
| `Chunk Coord` | `FIntVector` (Integer Vector) — The chunk coordinate to check. |
| **Outputs** | |
| `Return Value` | `Boolean` — `true` if the chunk's physics collision is cooked and active. |
| **Description** | Checks whether a chunk has active physics collision. When `true`, characters and physics objects can walk on and collide with this chunk's terrain. |

---

#### GetReadyChunkCount
| | |
|---|---|
| **Node Name** | `Get Ready Chunk Count` |
| **Category** | `Voxel\|Chunk` |
| **Type** | BlueprintPure (value node) |
| **Input** | *none* |
| **Output** | `Return Value` — `Integer` — Total chunks that are fully generated and loaded. |
| **Description** | Returns how many chunks are currently in the "ready" state (generation complete, mesh applied). Useful for loading screens or progress indicators. |

---

#### GetRequestedChunkCount
| | |
|---|---|
| **Node Name** | `Get Requested Chunk Count` |
| **Category** | `Voxel\|Chunk` |
| **Type** | BlueprintPure (value node) |
| **Input** | *none* |
| **Output** | `Return Value` — `Integer` — Total chunks currently tracked (generating + loaded). |
| **Description** | Returns the total number of chunks currently being tracked, including those still generating and those already loaded. Compare with `GetReadyChunkCount` to estimate loading progress. |

---

## 2. UVoxelStreamingManager — Blueprint Functions

Access via: **Get World Subsystem → Voxel Streaming Manager**

Controls which chunks load/unload based on viewer distance.

---

### Category: `Voxel|Streaming`

#### ApplyPreset
| | |
|---|---|
| **Node Name** | `Apply Preset` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintCallable |
| **Input** | `Preset` — A `UVoxelStreamingPreset` data asset. Create one in Content Browser → Miscellaneous → Data Asset → VoxelStreamingPreset. |
| **Output** | *none* |
| **Description** | Applies all distance bands and budget parameters from a preset asset. Equivalent to calling each `Set...Distance` and `SetStreamingBudgetMs` individually. |

---

#### SetViewerPosition
| | |
|---|---|
| **Node Name** | `Set Viewer Position` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintCallable |
| **Input** | `World Position` — `FVector` — A world-space location in Unreal units (cm). Get from `GetActorLocation`. |
| **Output** | *none* |
| **Description** | Overrides the auto-tracked viewer position. By default, the streaming manager follows the first local player's pawn. Call this to track a different actor (e.g. a spectator camera). |

---

#### SetSimulationDistance / GetSimulationDistance
| | |
|---|---|
| **Node Name** | `Set Simulation Distance` / `Get Simulation Distance` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintCallable / BlueprintPure |
| **Input (Set)** | `In Simulation Distance` — `Integer` — Distance in **chunk units** (NOT world units). |
| **Output (Get)** | `Return Value` — `Integer` — Current simulation distance in chunk units. |
| **Description** | Maximum distance from the viewer (in chunks) at which chunks have **physical collision**. Characters and physics objects only collide with terrain within this radius. Default: `4`. |

**Important:** All distance values are in **chunk units**. One chunk unit = `ChunkSize * VoxelWorldSize` cm. With defaults (ChunkSize=32, VoxelWorldSize=100), 1 chunk unit = 3200 cm = 32 meters.

---

#### SetRenderDistance / GetRenderDistance
| | |
|---|---|
| **Node Name** | `Set Render Distance` / `Get Render Distance` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintCallable / BlueprintPure |
| **Input (Set)** | `In Render Distance` — `Integer` — Distance in chunk units. |
| **Output (Get)** | `Return Value` — `Integer` |
| **Description** | Maximum distance from the viewer at which chunk meshes are **visible**. Chunks beyond this are hidden but may still exist in memory. Default: `8`. |

---

#### SetGenerationDistance / GetGenerationDistance
| | |
|---|---|
| **Node Name** | `Set Generation Distance` / `Get Generation Distance` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintCallable / BlueprintPure |
| **Input (Set)** | `In Generation Distance` — `Integer` — Distance in chunk units. |
| **Output (Get)** | `Return Value` — `Integer` |
| **Description** | Maximum distance from the viewer at which new chunks are **generated and meshed**. Should be >= RenderDistance so chunks are ready before becoming visible. Default: `10`. |

---

#### SetPersistenceDistance / GetPersistenceDistance
| | |
|---|---|
| **Node Name** | `Set Persistence Distance` / `Get Persistence Distance` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintCallable / BlueprintPure |
| **Input (Set)** | `In Persistence Distance` — `Integer` — Distance in chunk units. |
| **Output (Get)** | `Return Value` — `Integer` |
| **Description** | Maximum distance from the viewer at which chunks are **kept in memory**. Chunks beyond this are fully unloaded. Should be >= GenerationDistance to avoid re-generating chunks that were just created. Default: `12`. |

---

#### SetStreamingBudgetMs / GetStreamingBudgetMs
| | |
|---|---|
| **Node Name** | `Set Streaming Budget Ms` / `Get Streaming Budget Ms` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintCallable / BlueprintPure |
| **Input (Set)** | `In Budget Ms` — `Float` — Time budget in milliseconds per frame. |
| **Output (Get)** | `Return Value` — `Float` |
| **Description** | Maximum time (ms) the streaming manager can spend per frame on chunk load/unload operations. Lower values = less frame hitching but slower world loading. Default: `1.5` ms. |

---

#### GetManagedChunkCount
| | |
|---|---|
| **Node Name** | `Get Managed Chunk Count` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintPure |
| **Output** | `Return Value` — `Integer` |
| **Description** | Number of chunks currently being managed (requested + loaded) by the streaming system. |

---

#### GetVisibleChunkCount
| | |
|---|---|
| **Node Name** | `Get Visible Chunk Count` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintPure |
| **Output** | `Return Value` — `Integer` |
| **Description** | Number of chunks currently set to visible (within RenderDistance). |

---

#### GetPendingRequestCount
| | |
|---|---|
| **Node Name** | `Get Pending Request Count` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintPure |
| **Output** | `Return Value` — `Integer` |
| **Description** | Number of chunk generation requests queued but not yet dispatched (waiting for frame budget). |

---

#### GetPendingUnloadCount
| | |
|---|---|
| **Node Name** | `Get Pending Unload Count` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintPure |
| **Output** | `Return Value` — `Integer` |
| **Description** | Number of chunk unloads queued but not yet executed (waiting for frame budget). |

---

#### GetLastTickBudgetUsedMs
| | |
|---|---|
| **Node Name** | `Get Last Tick Budget Used Ms` |
| **Category** | `Voxel\|Streaming` |
| **Type** | BlueprintPure |
| **Output** | `Return Value` — `Float` — Milliseconds actually spent on streaming last frame. |
| **Description** | How much time (ms) the streaming manager actually consumed during its last tick. Compare with `GetStreamingBudgetMs` to see budget utilization. |

---

### Category: `Voxel|Development`

These functions are for **development and debugging only**. They are NOT intended for shipping gameplay logic.

#### SetStreamingFrozen / IsStreamingFrozen
| | |
|---|---|
| **Node Name** | `Set Streaming Frozen` / `Is Streaming Frozen` |
| **Category** | `Voxel\|Development` |
| **Type** | BlueprintCallable / BlueprintPure |
| **Input (Set)** | `bFrozen` — `Boolean` — `true` to freeze, `false` to resume. |
| **Output (Get)** | `Return Value` — `Boolean` |
| **Description** | Freezes all chunk loading/unloading. The existing chunks stay in place. Useful for steady-state rendering benchmarks (Mode D) or screenshot captures. |

---

#### ForceReevaluateQueue
| | |
|---|---|
| **Node Name** | `Force Reevaluate Queue` |
| **Category** | `Voxel\|Development` |
| **Type** | BlueprintCallable |
| **Description** | Forces the streaming manager to recompute all desired chunk coordinates on the next tick. Normally happens automatically on viewer movement. |

---

#### ClearAllManaged
| | |
|---|---|
| **Node Name** | `Clear All Managed` |
| **Category** | `Voxel\|Development` |
| **Type** | BlueprintCallable |
| **Description** | Drops all chunks managed by the streaming system. Used for baseline performance testing (Mode A). |

---

## 3. UVoxelConfigValidator — Blueprint Functions

Access via: Search **"Validate"** in any Blueprint graph. These are **static** functions — no subsystem reference needed.

### Category: `Voxel|Validation`

#### ValidateWorldDefinition
| | |
|---|---|
| **Node Name** | `Validate World Definition` |
| **Category** | `Voxel\|Validation` |
| **Type** | BlueprintCallable (static) |
| **Inputs** | |
| `World Def` | `UVoxelWorldDefinition*` — The world definition asset to validate. |
| `Chunk Size` | `Integer` — Expected chunk size (default: `32`). |
| `World Height In Chunks` | `Integer` — Expected world height (default: `8`). |
| **Output** | `Return Value` — `Array of FVoxelValidationMessage` — Each entry has `Severity` (Info/Warning/Error), `Message`, and `Suggestion`. |
| **Description** | Audits a World Definition for missing generation definitions, invalid soft references, and configuration issues. Returns actionable messages. |

---

#### ValidateGenerationDefinition
| | |
|---|---|
| **Node Name** | `Validate Generation Definition` |
| **Category** | `Voxel\|Validation` |
| **Type** | BlueprintCallable (static) |
| **Inputs** | |
| `Gen Def` | `UVoxelGenerationDefinition*` — The generation definition to validate. |
| `Chunk Size` | `Integer` — (default: `32`) |
| `World Height In Chunks` | `Integer` — (default: `8`) |
| **Output** | `Return Value` — `Array of FVoxelValidationMessage` |
| **Description** | Checks generation parameters for out-of-range values (e.g. BaseHeight exceeding world height bounds). |

---

#### ValidateBlockDefinitions
| | |
|---|---|
| **Node Name** | `Validate Block Definitions` |
| **Category** | `Voxel\|Validation` |
| **Type** | BlueprintCallable (static) |
| **Input** | `Block Defs` — `Array of UVoxelBlockDefinition*` |
| **Output** | `Return Value` — `Array of FVoxelValidationMessage` |
| **Description** | Checks for duplicate block IDs and reserved ID conflicts (Block ID 0 is reserved for Air). |

---

#### ValidateBiomeDefinition
| | |
|---|---|
| **Node Name** | `Validate Biome Definition` |
| **Category** | `Voxel\|Validation` |
| **Type** | BlueprintCallable (static) |
| **Input** | `Biome Def` — `UVoxelBiomeDefinition*` |
| **Output** | `Return Value` — `Array of FVoxelValidationMessage` |
| **Description** | Checks biome terrain layer configuration for missing or misconfigured block references. |

---

## 4. Data Assets — Editor Properties

These are **not** Blueprint functions. They are **Data Assets** you create in the Content Browser and configure in the Details panel. Every property has an in-editor tooltip.

### UVoxelWorldDefinition
**Create:** Content Browser → Right-click → Miscellaneous → Data Asset → **VoxelWorldDefinition**

| Property | Type | Category | Default | Description |
|---|---|---|---|---|
| `WorldName` | Text | World | *(empty)* | Human-readable name for display/debugging. |
| `WorldSeed` | Integer | World | `1234` | Master seed for deterministic generation. |
| `VoxelWorldSize` | Float | World | `100.0` | Size of one voxel in cm. 100 = 1 meter. |
| `GenerationDefinition` | Soft Ref | Generation | *null* | Points to a `UVoxelGenerationDefinition` asset. |
| `Biomes` | Array of Soft Refs | Biomes | *empty* | Array of `UVoxelBiomeDefinition` assets. |
| `StreamingPreset` | Soft Ref | Streaming | *null* | Points to a `UVoxelStreamingPreset` asset. |
| `PhysicsPreset` | Soft Ref | Physics | *null* | Points to a `UVoxelPhysicsPreset` asset. |
| `BlockMaterials` | Map\<Int, Soft Ref\> | Rendering | *empty* | Block ID → Material mapping. |
| `DefaultMaterial` | Soft Ref | Rendering | *null* | Fallback material for unmapped block IDs. |

---

### UVoxelGenerationDefinition
**Create:** Content Browser → Right-click → Miscellaneous → Data Asset → **VoxelGenerationDefinition**

#### Climate Section
| Property | Type | Default | Description |
|---|---|---|---|
| `Frequency` | Float | `0.001` | Climate noise frequency. Lower = larger biome regions. |
| `TemperatureSeedOffset` | Integer | `1000` | Seed offset for the temperature noise channel. |
| `HumiditySeedOffset` | Integer | `2000` | Seed offset for the humidity noise channel. |

#### Terrain Section
| Property | Type | Default | Description |
|---|---|---|---|
| `BaseHeight` | Integer | `64` | Voxel height around which terrain oscillates. |
| `HeightAmplitude` | Float | `40.0` | Max height variation in voxels. Higher = more dramatic terrain. |
| `BaseFrequency` | Float | `0.01` | Noise frequency. Lower = smoother terrain. |
| `NoiseOctaves` | Integer | `4` | FBM octave count (1-8). More = finer detail, more CPU. |
| `Lacunarity` | Float | `2.0` | Frequency multiplier between octaves. |
| `Persistence` | Float | `0.5` | Amplitude multiplier between octaves. |
| `FallbackStoneBlock` | Soft Ref | *null* | Block definition for deep underground stone. |
| `FallbackDirtBlock` | Soft Ref | *null* | Block definition for the dirt layer. |
| `FallbackGrassBlock` | Soft Ref | *null* | Block definition for the surface grass layer. |
| `FallbackDirtDepth` | Integer | `4` | Dirt layer thickness in voxels before stone. |

#### Caves Section
| Property | Type | Default | Description |
|---|---|---|---|
| `bEnabled` | Boolean | `true` | Enable/disable cave generation entirely. |
| `CarveThreshold` | Float | `0.58` | Noise threshold for carving (0-1). Higher = smaller caves. |
| `DensityFrequency` | Float | `0.045` | 3D cave noise frequency. Lower = larger cave networks. |
| `NoiseOctaves` | Integer | `3` | Cave noise octave count (1-8). |
| `SurfaceProtectionDepth` | Integer | `3` | Voxels below surface where caves cannot carve. |
| `CaveSeedOffset` | Integer | `5000` | Seed offset for cave noise channel. |

---

### UVoxelStreamingPreset
**Create:** Content Browser → Right-click → Miscellaneous → Data Asset → **VoxelStreamingPreset**

| Property | Type | Category | Default | Description |
|---|---|---|---|---|
| `SimulationDistance` | Integer | Distances | `4` | Max distance (chunks) for physics collision. |
| `RenderDistance` | Integer | Distances | `8` | Max distance (chunks) for mesh visibility. |
| `GenerationDistance` | Integer | Distances | `10` | Max distance (chunks) for chunk generation. |
| `PersistenceDistance` | Integer | Distances | `12` | Max distance (chunks) before full unload. |
| `StreamingBudgetMs` | Float | Budget | `1.5` | Frame time budget (ms) for streaming ops. |
| `MaxMeshFinalizationsPerTick` | Integer | Budget | `4` | Max mesh GPU uploads per tick. |
| `MaxCollisionFinalizationsPerTick` | Integer | Budget | `4` | Max collision cooks per tick. |

---

### UVoxelPhysicsPreset
**Create:** Content Browser → Right-click → Miscellaneous → Data Asset → **VoxelPhysicsPreset**

| Property | Type | Default | Description |
|---|---|---|---|
| `CollisionMode` | Enum | `Complex` | Collision fidelity. Only `Complex` (full triangle mesh) is implemented. |
| `bAsyncCooking` | Boolean | `true` | Cook collision meshes off the Game Thread. Strongly recommended. |
| `CollisionProfileName` | Name | `BlockAll` | Unreal collision profile applied to voxel collision components. |

---

## 5. Blueprint-Exposed Enums

### EVoxelValidationSeverity
| Value | Description |
|---|---|
| `Info` | Informational note (non-blocking). |
| `Warning` | Potential issue that may cause unexpected behavior. |
| `Error` | Critical issue that will prevent correct operation. |

---

## 6. Blueprint-Exposed Structs

### FVoxelValidationMessage
| Field | Type | Description |
|---|---|---|
| `Severity` | `EVoxelValidationSeverity` | Info, Warning, or Error. |
| `Message` | `String` | Human-readable description of the issue. |
| `Suggestion` | `String` | Actionable fix suggestion (may be empty for info). |

---

## 7. Quick Reference — Where to Find Things

| I want to... | Blueprint Search Term | Category |
|---|---|---|
| Get block at world position | `Try Get Block at World Position` | `Voxel\|Query` |
| Check if block is solid | `Try Is Solid at World Position` | `Voxel\|Query` |
| Convert world pos to chunk coord | `World Position to Chunk Coordinate` | `Voxel\|Query` |
| Check if chunk is loaded | `Is Chunk Loaded` | `Voxel\|Chunk` |
| Check if collision is ready | `Is Chunk Collision Ready` | `Voxel\|Chunk` |
| Get number of loaded chunks | `Get Ready Chunk Count` | `Voxel\|Chunk` |
| Apply a world definition | `Apply World Definition` | `Voxel\|World` |
| Clear all chunks | `Clear All Chunks` | `Voxel\|World` |
| Set render distance at runtime | `Set Render Distance` | `Voxel\|Streaming` |
| Set simulation distance | `Set Simulation Distance` | `Voxel\|Streaming` |
| Override viewer position | `Set Viewer Position` | `Voxel\|Streaming` |
| Apply streaming preset | `Apply Preset` | `Voxel\|Streaming` |
| Validate world definition | `Validate World Definition` | `Voxel\|Validation` |
| Freeze streaming (debug) | `Set Streaming Frozen` | `Voxel\|Development` |

---

## 8. Common Blueprint Patterns

### Print Current Chunk Coordinate
```
Event Tick → GetActorLocation
           → Get World Subsystem (VoxelWorldSubsystem)
           → WorldPositionToChunkCoordinate
           → Break IntVector
           → Format String ("Chunk: {X}, {Y}, {Z}")
           → Print String
```

### Check if Player's Chunk Has Collision
```
Event Tick → GetActorLocation
           → Get World Subsystem (VoxelWorldSubsystem)
           → WorldPositionToChunkCoordinate
           → IsChunkCollisionReady
           → Branch
               ├─ True → (collision active, player can walk)
               └─ False → (still loading, show indicator)
```

### Loading Progress Bar
```
Event Tick → Get World Subsystem (VoxelWorldSubsystem)
           → GetReadyChunkCount / GetRequestedChunkCount
           → Divide (Float)
           → Set Progress Bar Percent
```
