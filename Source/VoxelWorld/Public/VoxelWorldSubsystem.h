// VoxelWorldSubsystem.h
//
// Purpose:
//   The integration point: given a chunk coordinate, produces a rendered
//   chunk asynchronously, hiding generation + meshing + component/material
//   setup behind one call. This is the piece VoxelDebug fakes by hand
//   (calling PrecacheBiomeLayers per-call, running generation/meshing
//   synchronously on the Game Thread, manually managing preview
//   components) - this class does it once, correctly, for real use.
//
// Explicit scope boundary (see TODO.md / Docs/ARCHITECTURE.md):
//   - This class does NOT decide WHEN or WHY to request a chunk (no
//     distance-to-player logic, no automatic loading) - that is
//     VoxelStreaming's job, not yet built. RequestChunk is called
//     externally, by whatever decides chunks are needed.
//   - This class does NOT stitch mesh seams across chunk boundaries -
//     FVoxelMesher already documents that gap, and it is unchanged here.
//   - This class does NOT implement job cancellation - if UnloadChunk is
//     called while generation/meshing is still in flight for that
//     coordinate, the in-flight job still completes and its result is
//     silently discarded (see OnChunkMeshReady). Wiring real cancellation
//     through here is explicitly deferred to VoxelStreaming, consistent
//     with EVoxelJobState::Cancelled existing but being unused everywhere
//     so far.
//
// Responsibilities:
//   - Own the FVoxelChunkStore for this world
//   - Call UVoxelBlockRegistry::PrecacheBiomeLayers ONCE at Initialize,
//     not per chunk request
//   - RequestChunk: reserve a chunk slot synchronously, dispatch
//     generation+meshing as one worker-thread job via FVoxelScheduler,
//     marshal the result back to the Game Thread to create/update a
//     UVoxelMeshComponent
//   - UnloadChunk: remove the chunk's storage and rendering
//
// Thread ownership: all PUBLIC methods are Game-Thread-only (matches
//   FVoxelChunkStore's own current Game-Thread-only contract - see its
//   header). The dispatched generation+meshing work runs on a worker
//   thread; only the final component update happens back on the Game
//   Thread, via an explicit marshal (FVoxelScheduler's OnComplete does NOT
//   guarantee Game Thread - see VoxelScheduler.h).
//
// Dependencies: VoxelRuntime (FVoxelScheduler), VoxelAssets
//   (UVoxelBlockRegistry), VoxelStorage (FVoxelChunkStore),
//   VoxelGeneration (FVoxelGenerationPipeline), VoxelMeshing
//   (FVoxelMesher), VoxelRendering (UVoxelMeshComponent).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VoxelCoreTypes.h"
#include "VoxelChunkStore.h"
#include "VoxelWorldSubsystem.generated.h"
class FVoxelChunk;
class UVoxelBlockRegistry;
class UVoxelBiomeDefinition;
class UVoxelMeshComponent;
class UMaterialInterface;
class AVoxelWorldRenderActor;
struct FVoxelMeshData;

UCLASS()
class VOXELWORLD_API UVoxelWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual ~UVoxelWorldSubsystem() override;

	/**
	 * Reserves storage for Coordinate (synchronous, cheap) and dispatches
	 * generation+meshing asynchronously. Idempotent - calling again for an
	 * already-requested-or-loaded coordinate just returns the existing
	 * handle without dispatching a second job.
	 */
	FVoxelChunkHandle RequestChunk(const FVoxelChunkCoordinate& Coordinate);

	/** Removes the chunk's storage and rendering. Does not cancel an in-flight job for this coordinate - see class header. */
	void UnloadChunk(const FVoxelChunkCoordinate& Coordinate);

	/** Read-only access to already-generated chunk data. Returns nullptr if not requested, still generating, or unloaded. */
	const FVoxelChunk* FindChunk(const FVoxelChunkCoordinate& Coordinate) const;

	/** True once the chunk has been generated (mesh may still be empty for an all-air chunk - that's a valid ready state, not a pending one). */
	bool IsChunkReady(const FVoxelChunkCoordinate& Coordinate) const;

	int32 GetChunkSize() const { return ChunkSize; }
	int32 GetWorldSeed() const { return WorldSeed; }

private:
	void OnChunkMeshReady(FVoxelChunkCoordinate Coordinate, FVoxelMeshData&& MeshData);
	UVoxelMeshComponent* GetOrCreateMeshComponent(const FVoxelChunkCoordinate& Coordinate);
	UMaterialInterface* ResolveMaterialForId(int32 MaterialId) const;

	TUniquePtr<FVoxelChunkStore> ChunkStore;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelBlockRegistry> BlockRegistry;

	// Strong refs so resolved biomes stay loaded for the subsystem's lifetime.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelBiomeDefinition>> ResolvedBiomes;
	TArray<const UVoxelBiomeDefinition*> AvailableBiomes; // raw-pointer view of ResolvedBiomes, for passing into generation calls

	UPROPERTY(Transient)
	TObjectPtr<AVoxelWorldRenderActor> RenderHostActor;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UMaterialInterface>> ResolvedBlockMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResolvedDefaultMaterial;

	// Not a UPROPERTY: FVoxelChunkCoordinate is a plain struct (not USTRUCT),
	// so UHT cannot parse it as a TMap key. GC safety is fine because each
	// component's Outer is RenderHostActor, which keeps it rooted.
	TMap<FVoxelChunkCoordinate, TWeakObjectPtr<UVoxelMeshComponent>> ChunkMeshComponents;

	TSet<FVoxelChunkCoordinate> RequestedCoordinates; // both in-flight and completed - prevents double-dispatch
	TSet<FVoxelChunkCoordinate> ReadyCoordinates;     // generation completed (mesh may still be empty for all-air chunks)

	int32 ChunkSize = 32;
	int32 WorldSeed = 1234;
	float VoxelWorldSize = 100.0f;
};
