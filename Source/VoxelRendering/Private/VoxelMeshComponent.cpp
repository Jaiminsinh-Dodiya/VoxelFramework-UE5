// VoxelMeshComponent.cpp

#include "VoxelMeshComponent.h"
#include "VoxelMeshSceneProxy.h"
#include "VoxelMeshData.h"

UVoxelMeshComponent::UVoxelMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	MeshData = MakeShared<FVoxelMeshData, ESPMode::ThreadSafe>();
	LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);
}

void UVoxelMeshComponent::SetMeshData(FVoxelMeshData&& InMeshData)
{
	MeshData = MakeShared<FVoxelMeshData, ESPMode::ThreadSafe>(MoveTemp(InMeshData));
	UpdateLocalBounds();
	MarkRenderStateDirty(); // triggers CreateSceneProxy on the next render state update
}

void UVoxelMeshComponent::ClearMeshData()
{
	MeshData = MakeShared<FVoxelMeshData, ESPMode::ThreadSafe>();
	LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);
	MarkRenderStateDirty();
}

void UVoxelMeshComponent::UpdateLocalBounds()
{
	if (MeshData.IsValid() && MeshData->Bounds.IsValid)
	{
		LocalBounds = FBoxSphereBounds(MeshData->Bounds);
	}
	else
	{
		LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);
	}
}

FPrimitiveSceneProxy* UVoxelMeshComponent::CreateSceneProxy()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_SceneProxyCreate);

	if (!MeshData.IsValid() || MeshData->IsEmpty())
	{
		return nullptr; // engine handles a null proxy correctly - component just draws nothing
	}

	return new FVoxelMeshSceneProxy(this);
}

FBoxSphereBounds UVoxelMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return LocalBounds.TransformBy(LocalToWorld);
}

int32 UVoxelMeshComponent::GetNumMaterials() const
{
	return MeshData.IsValid() ? MeshData->Sections.Num() : 0;
}
