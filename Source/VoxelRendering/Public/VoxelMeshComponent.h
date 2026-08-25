// VoxelMeshComponent.h
//
// Purpose:
//   Production rendering component consuming FVoxelMeshData, per ADR-004.
//   This is what VoxelDebug's UProceduralMeshComponent exception exists to
//   be replaced by eventually - this component builds a real custom
//   FPrimitiveSceneProxy (FVoxelMeshSceneProxy) rather than going through
//   PMC's generic runtime-mesh machinery.
//
// Responsibilities:
//   - Own a per-section material list and the current FVoxelMeshData
//   - Build/rebuild the scene proxy when mesh data changes
//   - Report bounds and material usage to the engine
//
// Thread ownership:
//   SetMeshData must be called from the Game Thread (standard UActorComponent
//   rule). The actual GPU resource creation happens via ENQUEUE_RENDER_COMMAND
//   inside FVoxelMeshSceneProxy's constructor, which itself runs on the
//   render thread as part of MarkRenderStateDirty's normal proxy-recreation
//   flow - this class does not manage that threading itself, it relies on
//   the engine's existing UPrimitiveComponent/FPrimitiveSceneProxy lifecycle.
//
// Dependencies: Engine (UMeshComponent, FPrimitiveSceneProxy), VoxelMeshing
//   (FVoxelMeshData). Deliberately NOT dependent on VoxelStorage,
//   VoxelGeneration, or VoxelAssets - this component only knows how to draw
//   what it's handed, nothing about where that data came from.
//
// Known gaps (see Docs/TODO.md):
//   - Async upload not implemented: SetMeshData rebuilds the scene proxy
//     synchronously via MarkRenderStateDirty, which stalls until the next
//     render state update. A truly async upload path (build GPU resources
//     on a worker thread, swap in atomically) is future work.
//   - No LOD support yet - one section set, one detail level.
//   - No frustum/occlusion culling beyond what FPrimitiveSceneProxy's
//     default bounds-based visibility already provides for free.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "VoxelMeshComponent.generated.h"

struct FVoxelMeshData;

UCLASS(ClassGroup = Rendering, meta = (BlueprintSpawnableComponent))
class VOXELRENDERING_API UVoxelMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UVoxelMeshComponent();

	/**
	 * Replaces the current mesh data and triggers a scene proxy rebuild.
	 * Safe to call repeatedly (e.g. on chunk edit) - each call fully
	 * replaces prior geometry, there is no incremental/partial update path.
	 */
	void SetMeshData(FVoxelMeshData&& MeshData);

	/** Clears geometry - component becomes invisible until SetMeshData is called again. */
	void ClearMeshData();

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UPrimitiveComponent Interface

	//~ Begin UMeshComponent Interface
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface

	/** Read-only access for FVoxelMeshSceneProxy's constructor. */
	const FVoxelMeshData& GetMeshData() const { return *MeshData; }

private:
	// TSharedPtr rather than a value member: FVoxelMeshSceneProxy's
	// constructor reads this on the Game Thread when CreateSceneProxy runs,
	// but the render-thread command it enqueues captures data by value from
	// there - see FVoxelMeshSceneProxy.cpp for exactly what's captured.
	TSharedPtr<FVoxelMeshData, ESPMode::ThreadSafe> MeshData;

	FBoxSphereBounds LocalBounds;
	void UpdateLocalBounds();
};
