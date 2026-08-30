# Voxel Framework API Reference

This document provides a comprehensive reference for the Voxel Framework API, organized by module.

## VoxelCore

### FVoxelChunkCoordinate
- `X`, `Y`, `Z`: `int32`
- `ChebyshevDistanceTo(const FVoxelChunkCoordinate& Other) const -> int32`
- `operator==(const FVoxelChunkCoordinate& Other) const -> bool`
- `operator+(const FVoxelChunkCoordinate& Other) const -> FVoxelChunkCoordinate`
- `GetTypeHash(const FVoxelChunkCoordinate& Coordinate) -> uint32`

### FVoxelChunkHandle
- `Coordinate`: `FVoxelChunkCoordinate`
- `Generation`: `uint32`
- `IsValid() const -> bool`
- `operator==(const FVoxelChunkHandle& Other) const -> bool`
- `GetTypeHash(const FVoxelChunkHandle& Handle) -> uint32`

### FVoxelBlockId
- `FVoxelBlockId`: `uint16`
- `VoxelBlockId_Air`: `0`

### EVoxelJobState
- `Queued`
- `Running`
- `Completed`
- `Cancelled`

### FVoxelJobHandle
- `JobId`: `uint32`

## VoxelRuntime

### FVoxelRuntimeModule
- `Get() -> FVoxelRuntimeModule&`
- `IsAvailable() -> bool`
- `GetScheduler() -> FVoxelScheduler*`

### FVoxelScheduler
- `Submit(...) -> FVoxelJobHandle`
- `GetState(FVoxelJobHandle) -> EVoxelJobState`
- `RequestCancel(FVoxelJobHandle)`

### EVoxelWorkPriority
- `Low`
- `Normal`
- `High`
- `Critical`

### UVoxelRuntimeSettings
| Property | Type | Default | Notes |
|---|---|---|---|
| ChunkSize | int32 | 32 | |
| WorldHeightInChunks | int32 | 8 | |
| SimulationDistance | int32 | 4 | |
| RenderDistance | int32 | 8 | |
| GenerationDistance | int32 | 10 | |
| PersistenceDistance | int32 | 12 | |
| StreamingBudgetMs | float | 1.5 | |
| RenderSubmissionBudgetMs | float | 1.0 | |
| MemoryBudgetMB | int32 | 256 | |

## VoxelMath

### VoxelNoise namespace
- `Sample2D(...)`
- `Sample3D(...)`
- `FBM2D(...)`
- `FBM3D(...)`

## VoxelAssets

### UVoxelBlockDefinition
| Property | Type | Notes |
|---|---|---|
| BlockId | FVoxelBlockId | |
| DisplayName | FString | |
| bIsSolid | bool | |
| bGeneratesCollision | bool | |
| MaterialLayerIndex | int32 | |
| VertexTint | FLinearColor | |

### UVoxelBiomeDefinition
| Property | Type | Notes |
|---|---|---|
| DisplayName | FString | |
| TemperatureRange | FVector2D | |
| HumidityRange | FVector2D | |
| TerrainLayers | TArray<FVoxelTerrainLayer> | |
| AmbientTint | FLinearColor | |
| BiomeTags | FGameplayTagContainer | |
| VegetationDensity | float | |

### FVoxelTerrainLayer (USTRUCT)
| Property | Type | Notes |
|---|---|---|
| Block | TSoftObjectPtr<UVoxelBlockDefinition> | |
| ThicknessVoxels | int32 | |

### UVoxelBlockRegistry
- `BuildFromDefinitions()`
- `FindDefinition(FVoxelBlockId)`
- `IsSolid(FVoxelBlockId)`
- `PrecacheBiomeLayers()`
- `GetResolvedLayerBlockIds()`

## VoxelStorage

### FVoxelChunk
- `FVoxelChunk(int32 InSize)`
- `ResetForReuse()`
- `GetSize() const -> int32`
- `GetBlock(...) const -> FVoxelBlockId`
- `SetBlock(..., bool bIsGenerationWrite)`
- `IsEmpty() const -> bool`
- `GetModifications() -> ...`
- `ApplyModifications(...)`
- `ToLinearIndex(...) const -> int32`

### FVoxelChunkStore
- `FVoxelChunkStore(int32 InChunkSize)`
- `CreateOrGetChunk(...) -> FVoxelChunkHandle`
- `RemoveChunk(...)`
- `FindChunkByCoordinate(...) -> FVoxelChunkHandle`
- `FindChunkByHandle(...) -> const FVoxelChunk*`
- `GetLoadedChunkCount() const -> int32`
Note: Move-only.

## VoxelGeneration

### IVoxelGenerationPass
- `GetPassName() const -> FString`
- `Execute(...)`

### FVoxelGenerationContext
- `WorldSeed`
- `ChunkCoordinate`
- `ChunkSize`
- `BlockRegistry`
- `AvailableBiomes`
- `Columns`
- `InitColumns()`
- `ColumnAt(...) -> FVoxelColumnData&`
- `LocalToWorldColumn(...)`

### FVoxelColumnData
- `Temperature`
- `Humidity`
- `TerrainHeight`
- `Biome`

### FVoxelGenerationPipeline
- `FVoxelGenerationPipeline()` (default pass order)
- `GenerateChunk(seed, coord, size, registry, biomes, outChunk)`
Note: Move-only.

### Generation Passes
| Pass | Reads | Writes |
|---|---|---|
| ClimatePass | None | Temperature, Humidity |
| BiomePass | Temperature, Humidity | Biome |
| TerrainPass | Biome | TerrainHeight, blocks |
| CavePass | Blocks | Blocks (carves air) |

### Tunable Constants
- **TerrainPass**: `NoiseOctaves=4`, `BaseFrequency=0.01f`, `HeightAmplitude=40.0f`, `BaseHeight=64`
- **CavePass**: `NoiseOctaves=3`, `DensityFrequency=0.045f`, `CarveThreshold=0.58f`, `SurfaceProtectionDepth=3`

## VoxelMeshing

### FVoxelMeshVertex
- `Position`
- `Normal`
- `UV`
- `Color` (baked AO)

### FVoxelMeshSection
- `MaterialId`
- `Indices`

### FVoxelMeshData
- `Vertices`
- `Sections`
- `IsEmpty() const -> bool`
- `GetTotalTriangleCount() const -> int32`

### FVoxelMesher
- `static GenerateMesh(chunk, registry) -> FVoxelMeshData`

### VoxelMeshingService
- `RequestMeshAsync(chunk, registry, onComplete)`

## VoxelRendering

Header: `VoxelMeshComponent.h`

### UVoxelMeshComponent (UCLASS)
Parent: `UMeshComponent`, ClassGroup=`Rendering`, BlueprintSpawnableComponent.
The production rendering component. Creates a custom `FVoxelMeshSceneProxy` to render `FVoxelMeshData` on the GPU.

#### Public methods:
```cpp
void SetMeshData(FVoxelMeshData&& InMeshData);  // Takes ownership, triggers MarkRenderStateDirty → CreateSceneProxy
void ClearMeshData();                           // Clears mesh, releases GPU resources
int32 GetNumMaterials() const override;         // Returns number of sections in current mesh data
```

#### Overrides from UPrimitiveComponent:
- `CreateSceneProxy()` — creates `FVoxelMeshSceneProxy`
- `CalcBounds()` — computes from vertex positions
- `GetNumMaterials()` — section count

Header: `VoxelMeshSceneProxy.h`

### FVoxelMeshSceneProxy (FPrimitiveSceneProxy)
Custom scene proxy that uploads `FVoxelMeshData` to GPU vertex/index buffers using `FStaticMeshVertexBuffers` and `FLocalVertexFactory`. Handles:
- Vertex buffer upload via ENQUEUE_RENDER_COMMAND
- Per-section draw calls with material assignment
- Proper cleanup of GPU resources on destruction

This is the production rendering path per ADR-004 — `FVoxelMeshData` (plain CPU arrays from VoxelMeshing) is consumed here and turned into GPU-visible geometry without VoxelMeshing knowing anything about rendering.

## VoxelPhysics

Header: `VoxelPhysicsTypes.h`

### EVoxelCollisionMode (enum class)
- `None`: No physical collision generated
- `Complex`: High-fidelity triangle collision for character gameplay

### EVoxelCollisionState (enum class)
- `NotRequired`: Outside simulation distance
- `Queued`: Dispatched/waiting for worker generation
- `Building`: Worker thread actively computing collision geometry
- `Cooking`: Game Thread / Chaos physics worker cooking `UBodySetup`
- `Ready`: Cooked and registered in Chaos `FPhysScene`
- `Unloading`: Teardown

Header: `VoxelCollisionData.h`

### FVoxelCollisionData (struct)
- `Vertices`: `TArray<FVector3f>`
- `Indices`: `TArray<FTriIndices>`
- `Bounds`: `FBox`
- `Coordinate`: `FVoxelChunkCoordinate`
- `CollisionRevision`: `uint32`
- `bIsEmpty`: `bool`
- `IsEmpty() const -> bool`
- `GetTriangleCount() const -> int32`
- `GetVertexCount() const -> int32`

Header: `VoxelCollisionBuilder.h`

### FVoxelCollisionBuilder
- `static BuildCollisionData(...) -> FVoxelCollisionData`: Pure worker-side greedy collision builder with neighbor face culling and block solidity filtering.

Header: `VoxelCollisionComponent.h`

### UVoxelCollisionComponent (UCLASS)
Parent: `UPrimitiveComponent`, `IInterface_CollisionDataProvider`, ClassGroup=`VoxelPhysics`.
- `SetCollisionData(FVoxelCollisionData&& InCollisionData, bool bAsyncCook = true)`: Installs collision snapshot and triggers Chaos physics cooking.
- `ClearCollisionData()`: Tears down active physics state.
- `HasActiveCollision() const -> bool`
- `GetCurrentCollisionRevision() const -> uint32`

## VoxelWorld

Header: `VoxelWorldSubsystem.h`

### UVoxelWorldSubsystem (UCLASS)
Parent: `UTickableWorldSubsystem`
The integration point: given a chunk coordinate, produces a rendered chunk asynchronously.

#### Public methods:
```cpp
virtual void Initialize(FSubsystemCollectionBase& Collection) override;
virtual void Deinitialize() override;
virtual ~UVoxelWorldSubsystem() override;

virtual void Tick(float DeltaTime) override;
virtual TStatId GetStatId() const override;

FVoxelChunkHandle RequestChunk(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority = EVoxelWorkPriority::Normal);
// Reserves storage (sync, cheap), dispatches generation+meshing async.
// Idempotent — re-requesting returns existing handle, no second job.

void UnloadChunk(const FVoxelChunkCoordinate& Coordinate, bool bTriggerNeighborRemesh = true);
// Removes chunk storage and rendering. Cancels in-flight scheduler job and triggers neighbor boundary remesh.

const FVoxelChunk* FindChunk(const FVoxelChunkCoordinate& Coordinate) const;
// Read-only access. Returns nullptr if not requested/still generating/unloaded.

bool IsChunkReady(const FVoxelChunkCoordinate& Coordinate) const;
// True once generation and meshing have completed.

EVoxelChunkState GetChunkState(const FVoxelChunkCoordinate& Coordinate) const;
// Authoritative lifecycle state of a chunk coordinate.

void RequestRemeshChunk(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority = EVoxelWorkPriority::Normal);
// Asynchronously remeshes an existing ready chunk to update boundary seams without re-generating voxels.

void RequestChunkCollision(const FVoxelChunkCoordinate& Coordinate, EVoxelWorkPriority WorkPriority = EVoxelWorkPriority::High);
// Requests physical collision generation and Chaos cooking for an existing resident chunk.

void UnloadChunkCollision(const FVoxelChunkCoordinate& Coordinate);
// Unloads and destroys collision representation for a chunk.

EVoxelCollisionState GetChunkCollisionState(const FVoxelChunkCoordinate& Coordinate) const;
// Returns current collision lifecycle state for a chunk coordinate.

bool HasChunkCollision(const FVoxelChunkCoordinate& Coordinate) const;
// Returns true if chunk currently has registered active physical collision.

void SetChunkVisible(const FVoxelChunkCoordinate& Coordinate, bool bVisible);
// Toggles mesh component visibility.

void SetCastShadows(bool bInCastShadows);
// Sets dynamic shadow casting across all voxel mesh components.

void SetCpuOnlyMode(bool bInCpuOnly);
// Bypasses render component creation and GPU uploads to isolate CPU throughput (Mode C).

void ClearAllChunks();
// Unloads all active chunks and cancels in-flight jobs.

int32 GetChunkSize() const;
int32 GetWorldSeed() const;
int32 GetReadyChunkCount() const;
int32 GetRequestedChunkCount() const;
int32 GetFinalizationQueueDepth() const;
float GetLastFinalizeBudgetUsedMs() const;
int32 GetLastFinalizeCount() const;
```

#### Component Pool & Latency Telemetry Accessors:
```cpp
int32 GetActiveComponentCount() const;
int32 GetPooledComponentCount() const;
int32 GetCreatedComponentCount() const;
int32 GetReusedComponentCount() const;
int32 GetDestroyedComponentCount() const;
int32 GetPeakPoolSize() const;

float GetAverageQueueLatencyMs() const;
float GetMaxQueueLatencyMs() const;
float GetOldestQueueItemAgeMs() const;
float CalculateQueueLatencyPercentile(float Percentile) const;
```

Header: `VoxelWorldSettings.h`

### UVoxelWorldSettings (UCLASS)
UDeveloperSettings, Project Settings → Plugins → Voxel World

| Property | Type | Default | Notes |
|---|---|---|---|
| WorldSeed | int32 | 1234 | Passed to generation pipeline |
| DefaultBiomes | TArray<TSoftObjectPtr<UVoxelBiomeDefinition>> | empty | Resolved at subsystem init |
| VoxelWorldSize | float | 100.0 | World-space size of one voxel |
| BlockMaterials | TMap<int32, TSoftObjectPtr<UMaterialInterface>> | empty | Per-block material overrides |
| DefaultMaterial | TSoftObjectPtr<UMaterialInterface> | unset | Fallback material |

Header: `AVoxelWorldRenderActor.h`

### AVoxelWorldRenderActor (UCLASS)
Parent: `AActor`, NotBlueprintable, NotPlaceable
Transient host actor spawned by `UVoxelWorldSubsystem` to hold `UVoxelMeshComponents`. Not placed by users — created/destroyed automatically with the subsystem.

## VoxelStreaming

Header: `VoxelStreamingTypes.h`

### EVoxelStreamingBand (enum class)
- `Simulation` — distance <= SimulationDistance (immediate player interaction)
- `Render` — distance <= RenderDistance (visible, rendered)
- `Generation` — distance <= GenerationDistance (data generated + meshed)
- `Persistence` — distance <= PersistenceDistance (kept resident if modified)
- `OutOfRange` — distance > PersistenceDistance (eligible for unload)

### VoxelStreaming namespace (Free Functions)
- `ClassifyChunkDistance(ChebyshevDist, SimDist, RenderDist, GenDist, PersistDist) -> EVoxelStreamingBand`
- `ComputeDesiredCoordinates(ViewerChunk, GenDist, WorldHeightInChunks) -> TArray<FVoxelChunkCoordinate>`

Header: `VoxelStreamingManager.h`

### UVoxelStreamingManager (UCLASS)
Parent: `UTickableWorldSubsystem`
Tick-driven streaming manager that decides which chunks to load, unload, and render based on viewer position and runtime distance bands.

#### Public methods:
```cpp
void SetViewerPosition(const FVector& WorldPosition); // Pass FLT_MAX to restore auto-tracking
int32 GetManagedChunkCount() const;
int32 GetVisibleChunkCount() const;
int32 GetPendingRequestCount() const;
int32 GetPendingUnloadCount() const;
float GetLastTickBudgetUsedMs() const;

void SetStreamingFrozen(bool bFrozen); // Freezes streaming updates (Mode D)
bool IsStreamingFrozen() const;
void ForceReevaluateQueue();
void ClearAllManaged();

void SetRenderDistance(int32 InRenderDistance);
int32 GetRenderDistance() const;
void SetSimulationDistance(int32 InSimulationDistance);
int32 GetSimulationDistance() const;
void SetGenerationDistance(int32 InGenerationDistance);
int32 GetGenerationDistance() const;
void SetPersistenceDistance(int32 InPersistenceDistance);
int32 GetPersistenceDistance() const;

void SetStreamingBudgetMs(float InBudgetMs);
float GetStreamingBudgetMs() const;

EVoxelWorkPriority GetPriorityForCoordinate(const FVoxelChunkCoordinate& Coordinate, const FVoxelChunkCoordinate& ViewerChunk) const;
FORCEINLINE EVoxelWorkPriority GetPriorityForDistance(int32 Dist) const;
```

## VoxelDebug

Header: `VoxelDebugVisualizer.h`

### AVoxelDebugVisualizer (UCLASS)
Parent: `AActor`

Properties: `WorldSeed`, `ChunkSize`, `ChunkRadiusXY`, `ChunkCountZ`, `VoxelWorldSize`, `BlockMaterials`, `DefaultMaterial`, `Biomes`, `bEnableCollisionInMeshPreview`, `ActiveDiagnosticMode`, `bVoxelCastShadows`.

#### CallInEditor functions:
```cpp
UFUNCTION(CallInEditor, Category="Voxel Debug|Cube Preview")
void GenerateAndVisualize();           // cube-per-visible-voxel (ISMC)

UFUNCTION(CallInEditor, Category="Voxel Debug|Mesh Preview")
void GenerateAndVisualizeMeshed();     // real FVoxelMesher via PMC

UFUNCTION(CallInEditor, Category="Voxel Debug|Real Renderer Preview")
void GenerateAndVisualizeRendered();   // real UVoxelMeshComponent/FVoxelMeshSceneProxy

UFUNCTION(CallInEditor, Category="Voxel Debug|World Subsystem Test")
void RequestChunksViaSubsystem();      // async UVoxelWorldSubsystem pipeline (PIE only)

UFUNCTION(CallInEditor, Category="Voxel Debug|World Subsystem Test")
void ValidateSubsystemResults();       // per-chunk validation: readiness, retrieval, idempotency, data consistency

UFUNCTION(CallInEditor, Category="Voxel Debug")
void ClearVisualization();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance")
void StartPerformanceDiagnostics();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance")
void StopPerformanceDiagnostics();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance|Modes")
void ApplyModeA_Baseline();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance|Modes")
void ApplyModeB_VoxelRenderingOn();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance|Modes")
void ApplyModeC_CpuOnly();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance|Modes")
void ApplyModeD_StaticWorld();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance|Modes")
void ApplyModeE_StreamingStress();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance")
void ToggleVoxelShadows();

UFUNCTION(CallInEditor, Category="Voxel Debug|Performance")
void ResetDiagnosticStats();
```

## Cross-module data flow

### Direct / Manual Usage
```mermaid
sequenceDiagram
    participant Pipeline as FVoxelGenerationPipeline
    participant Store as FVoxelChunkStore
    participant Mesher as FVoxelMesher
    
    Note over Pipeline,Mesher: Manual synchronous usage
    Pipeline->>Store: Generate into chunk
    Store->>Mesher: Pass chunk for meshing
```

### Full Async Flow (UVoxelWorldSubsystem)
```mermaid
sequenceDiagram
    participant Subsystem as UVoxelWorldSubsystem
    participant Store as FVoxelChunkStore
    participant Scheduler as FVoxelScheduler
    participant Worker as Worker Thread
    participant Pipeline as FVoxelGenerationPipeline
    participant Mesher as FVoxelMesher
    participant GT as Game Thread
    participant Component as UVoxelMeshComponent

    Subsystem->>Store: CreateOrGetChunk(coordinate)
    Store-->>Subsystem: FVoxelChunkHandle
    Subsystem->>Scheduler: Submit(work, Normal, onComplete)
    Scheduler->>Worker: UE::Tasks::Launch
    Worker->>Pipeline: GenerateChunk(seed, coord, size, registry, biomes, chunk)
    Note over Pipeline: Climate → Biome → Terrain → Cave
    Worker->>Mesher: GenerateMesh(chunk, registry)
    Note over Mesher: Greedy mesh + AO
    Worker-->>Scheduler: task complete
    Scheduler->>Worker: OnComplete callback
    Worker->>GT: AsyncTask(GameThread, marshal)
    GT->>Component: SetMeshData(meshData)
    Note over Component: MarkRenderStateDirty → CreateSceneProxy
```
