// VoxelMeshSceneProxy.h
//
// Purpose:
//   Custom FPrimitiveSceneProxy consuming FVoxelMeshData directly, per
//   ADR-004 ("no ProceduralMeshComponent, no UMeshComponent generic runtime
//   mesh machinery for production rendering"). This IS the module the
//   original spec asked for - a dedicated rendering backend.
//
// Responsibilities:
//   - Build GPU-ready vertex/index buffers + a FLocalVertexFactory per
//     material section, from CPU-side FVoxelMeshData
//   - Submit FMeshBatch per section per relevant view in GetDynamicMeshElements
//   - Clean up render resources on destruction
//
// Thread ownership:
//   Constructor runs on the Game Thread (as all FPrimitiveSceneProxy
//   construction does, per engine convention) but populates GPU resources
//   via BeginInitResource, which defers actual RHI resource creation to the
//   render thread - this is the standard engine pattern, not something
//   specific to this class. Destructor is guaranteed by the engine to run
//   on the render thread. GetDynamicMeshElements runs on the render thread.
//
// Dependencies: RenderCore (FLocalVertexFactory, FStaticMeshVertexBuffers,
//   FDynamicMeshVertex), Engine (FPrimitiveSceneProxy), VoxelMeshing.
//
// Known gaps (see Docs/TODO.md):
//   - Always uses the dynamic-relevance draw path (GetDynamicMeshElements)
//     rather than registering into the static draw list - simpler and
//     correct, but not the most efficient path the engine offers for mesh
//     data that doesn't change every frame. A future optimization once
//     profiling shows this matters.
//   - No LOD - one section set per proxy, always drawn at full detail.
//   - Tangents are synthesized (arbitrary vector perpendicular to the
//     stored per-vertex Normal) since FVoxelMeshVertex doesn't carry an
//     authored tangent - fine for axis-aligned voxel-face normals with a
//     flat/simple material, would need revisiting for anything using
//     tangent-space effects (e.g. detail normal maps) later.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "LocalVertexFactory.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/ColorVertexBuffer.h"
#include "DynamicMeshBuilder.h"

class UVoxelMeshComponent;
class UMaterialInterface;

class FVoxelMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FVoxelMeshSceneProxy(UVoxelMeshComponent* Component);
	virtual ~FVoxelMeshSceneProxy() override;

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override;
	SIZE_T GetTypeHash() const override;

private:
	struct FSection
	{
		FStaticMeshVertexBuffers VertexBuffers;
		FDynamicMeshIndexBuffer32 IndexBuffer;
		FLocalVertexFactory VertexFactory;
		UMaterialInterface* Material = nullptr;
		int32 NumTriangles = 0;

		FSection() : VertexFactory(GMaxRHIFeatureLevel, "FVoxelMeshSceneProxy_FSection") {}
	};

	TArray<TUniquePtr<FSection>> Sections;
	UMaterialInterface* FallbackMaterial = nullptr;

	FMaterialRelevance MaterialRelevance;
};
