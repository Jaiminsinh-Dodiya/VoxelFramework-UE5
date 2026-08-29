// VoxelMeshSceneProxy.cpp
//
// See header for full design notes. This file follows the same overall
// shape Epic's own ProceduralMeshComponent plugin uses internally for its
// scene proxy (FDynamicMeshVertex + FStaticMeshVertexBuffers::
// InitFromDynamicVertex + FLocalVertexFactory + BeginInitResource per
// buffer) - that is a known-correct pattern for a custom dynamic-mesh
// FPrimitiveSceneProxy, adapted here to read from FVoxelMeshData instead of
// PMC's own section structures.

#include "VoxelMeshSceneProxy.h"
#include "VoxelMeshComponent.h"
#include "VoxelMeshData.h"
#include "Materials/MaterialInterface.h"
#include "MaterialDomain.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"

FVoxelMeshSceneProxy::FVoxelMeshSceneProxy(UVoxelMeshComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
{
	FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

	const FVoxelMeshData& MeshData = Component->GetMeshData();

	for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
	{
		const FVoxelMeshSection& SourceSection = MeshData.Sections[SectionIndex];
		if (SourceSection.Indices.Num() == 0)
		{
			continue;
		}

		TUniquePtr<FSection> Section = MakeUnique<FSection>();

		// Remap the shared MeshData.Vertices array into a compact,
		// section-local vertex buffer - same technique VoxelDebug's PMC
		// preview path uses, for the same reason (a vertex referenced by
		// only one material shouldn't bloat every other section's buffer).
		TArray<FDynamicMeshVertex> LocalVertices;
		TArray<uint32> LocalIndices;
		TMap<uint32, int32> GlobalToLocalVertexIndex;
		LocalVertices.Reserve(SourceSection.Indices.Num());
		LocalIndices.Reserve(SourceSection.Indices.Num());

		for (uint32 GlobalIndex : SourceSection.Indices)
		{
			int32 LocalIndex;
			if (const int32* Existing = GlobalToLocalVertexIndex.Find(GlobalIndex))
			{
				LocalIndex = *Existing;
			}
			else
			{
				const FVoxelMeshVertex& SourceVertex = MeshData.Vertices[GlobalIndex];

				FDynamicMeshVertex Vertex;
				Vertex.Position = SourceVertex.Position;
				Vertex.TextureCoordinate[0] = SourceVertex.UV;
				Vertex.Color = SourceVertex.Color;

				// Synthesized tangent
				const FVector3f Normal = SourceVertex.Normal;
				const FVector3f ArbitraryUp = (FMath::Abs(Normal.Z) < 0.99f) ? FVector3f::UpVector : FVector3f::ForwardVector;
				const FVector3f Tangent = FVector3f::CrossProduct(Normal, ArbitraryUp).GetSafeNormal();
				const FVector3f Bitangent = FVector3f::CrossProduct(Normal, Tangent);
				Vertex.SetTangents(Tangent, Bitangent, Normal);

				LocalIndex = LocalVertices.Add(Vertex);
				GlobalToLocalVertexIndex.Add(GlobalIndex, LocalIndex);
			}
			LocalIndices.Add(static_cast<uint32>(LocalIndex));
		}

		Section->VertexBuffers.InitFromDynamicVertex(&Section->VertexFactory, LocalVertices, /*NumTexCoords=*/1);
		Section->IndexBuffer.Indices = LocalIndices;
		Section->NumTriangles = LocalIndices.Num() / 3;

		UMaterialInterface* SectionMaterial = Component->GetMaterial(SectionIndex);
		Section->Material = SectionMaterial ? SectionMaterial : FallbackMaterial;

		// CPU-side data is fully populated above; these defer actual RHI
		// resource creation to the render thread, per standard engine
		// convention for FPrimitiveSceneProxy construction (see header).
		BeginInitResource(&Section->VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&Section->VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&Section->VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&Section->IndexBuffer);
		BeginInitResource(&Section->VertexFactory);

		Sections.Add(MoveTemp(Section));
	}
}

FVoxelMeshSceneProxy::~FVoxelMeshSceneProxy()
{
	// Guaranteed to run on the render thread by FPrimitiveSceneProxy's own
	// lifecycle contract - safe to call ReleaseResource directly here.
	for (const TUniquePtr<FSection>& Section : Sections)
	{
		Section->VertexBuffers.PositionVertexBuffer.ReleaseResource();
		Section->VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		Section->VertexBuffers.ColorVertexBuffer.ReleaseResource();
		Section->IndexBuffer.ReleaseResource();
		Section->VertexFactory.ReleaseResource();
	}
}

void FVoxelMeshSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}

		for (const TUniquePtr<FSection>& Section : Sections)
		{
			if (Section->NumTriangles == 0)
			{
				continue;
			}

			FMaterialRenderProxy* MaterialProxy = Section->Material->GetRenderProxy();

			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];

			BatchElement.IndexBuffer = &Section->IndexBuffer;
			Mesh.bWireframe = false;
			Mesh.VertexFactory = &Section->VertexFactory;
			Mesh.MaterialRenderProxy = MaterialProxy;

			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = Section->NumTriangles;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = Section->VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}

FPrimitiveViewRelevance FVoxelMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true; // see header "Known gaps" - static draw list path is future work
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = IsMovable() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

uint32 FVoxelMeshSceneProxy::GetMemoryFootprint() const
{
	// Approximate - does not walk every buffer's exact allocated size.
	return sizeof(*this);
}

SIZE_T FVoxelMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}
