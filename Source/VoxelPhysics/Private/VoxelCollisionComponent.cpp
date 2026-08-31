// VoxelCollisionComponent.cpp

#include "VoxelCollisionComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "VoxelPhysicsModule.h"

UVoxelCollisionComponent::UVoxelCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetGenerateOverlapEvents(false);
	bAlwaysCreatePhysicsState = true;
	LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);
}

UBodySetup* UVoxelCollisionComponent::CreateBodySetupHelper()
{
	UBodySetup* NewBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public | RF_ArchetypeObject : RF_NoFlags));
	NewBodySetup->BodySetupGuid = FGuid::NewGuid();
	NewBodySetup->bGenerateMirroredCollision = false;
	NewBodySetup->bDoubleSidedGeometry = false;
	NewBodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
	return NewBodySetup;
}

void UVoxelCollisionComponent::SetCollisionData(FVoxelCollisionData&& InCollisionData, bool bAsyncCook)
{
	check(IsInGameThread());

	CurrentRevision = InCollisionData.CollisionRevision;

	// Abort all in-flight async cooks for previous revisions
	for (TObjectPtr<UBodySetup>& PendingBody : PendingAsyncBodySetups)
	{
		if (IsValid(PendingBody))
		{
			PendingBody->AbortPhysicsMeshAsyncCreation();
		}
	}
	PendingAsyncBodySetups.Reset();

	// Empty collision fast-path: release physics state and do not allocate/cook BodySetup
	if (InCollisionData.IsEmpty())
	{
		CollisionData.Reset();
		BodySetup = nullptr;
		DestroyPhysicsState();
		LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);
		UpdateBounds();
		OnCollisionCookFinished.Broadcast(this, true, CurrentRevision);
		return;
	}

	CollisionData = MakeShared<FVoxelCollisionData, ESPMode::ThreadSafe>(MoveTemp(InCollisionData));
	LocalBounds = FBoxSphereBounds(CollisionData->Bounds.IsValid != 0 ? CollisionData->Bounds : FBox(FVector::ZeroVector, FVector::ZeroVector));
	UpdateBounds();

	UBodySetup* NewBodySetup = CreateBodySetupHelper();

	if (bAsyncCook)
	{
		PendingAsyncBodySetups.Add(NewBodySetup);
		NewBodySetup->CreatePhysicsMeshesAsync(
			FOnAsyncPhysicsCookFinished::CreateUObject(this, &UVoxelCollisionComponent::FinishPhysicsAsyncCook, NewBodySetup, CurrentRevision));
	}
	else
	{
		NewBodySetup->BodySetupGuid = FGuid::NewGuid();
		NewBodySetup->bHasCookedCollisionData = true;
		NewBodySetup->CreatePhysicsMeshes();
		BodySetup = NewBodySetup;
		RecreatePhysicsState();
		OnCollisionCookFinished.Broadcast(this, true, CurrentRevision);
	}
}

void UVoxelCollisionComponent::ClearCollisionData()
{
	check(IsInGameThread());

	for (TObjectPtr<UBodySetup>& PendingBody : PendingAsyncBodySetups)
	{
		if (IsValid(PendingBody))
		{
			PendingBody->AbortPhysicsMeshAsyncCreation();
		}
	}
	PendingAsyncBodySetups.Reset();

	CollisionData.Reset();
	BodySetup = nullptr;
	DestroyPhysicsState();
	LocalBounds = FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);
	UpdateBounds();
}

bool UVoxelCollisionComponent::HasActiveCollision() const
{
	return BodySetup != nullptr && CollisionData.IsValid() && !CollisionData->IsEmpty();
}

void UVoxelCollisionComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup, uint32 CookedRevision)
{
	check(IsInGameThread());

	if (CookedRevision != CurrentRevision || !IsValid(FinishedBodySetup))
	{
		// Stale revision cook completion discarded safely
		PendingAsyncBodySetups.Remove(FinishedBodySetup);
		return;
	}

	int32 FoundIdx = PendingAsyncBodySetups.Find(FinishedBodySetup);
	if (FoundIdx != INDEX_NONE)
	{
		if (bSuccess)
		{
			BodySetup = FinishedBodySetup;
			RecreatePhysicsState();

			// Remove finished and all prior superseded body setups
			TArray<TObjectPtr<UBodySetup>> RemainingQueue;
			for (int32 i = FoundIdx + 1; i < PendingAsyncBodySetups.Num(); ++i)
			{
				RemainingQueue.Add(PendingAsyncBodySetups[i]);
			}
			PendingAsyncBodySetups = MoveTemp(RemainingQueue);

			OnCollisionCookFinished.Broadcast(this, true, CookedRevision);
		}
		else
		{
			// Cook failure: do NOT install failed BodySetup!
			BodySetup = nullptr;
			DestroyPhysicsState();
			PendingAsyncBodySetups.RemoveAt(FoundIdx);

			OnCollisionCookFinished.Broadcast(this, false, CookedRevision);
		}
	}
}

bool UVoxelCollisionComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* OutCollisionData, bool InUseAllTriData)
{
	if (!CollisionData.IsValid() || CollisionData->IsEmpty() || !OutCollisionData)
	{
		return false;
	}

	OutCollisionData->Vertices = CollisionData->Vertices;
	OutCollisionData->Indices = CollisionData->Indices;
	OutCollisionData->bFlipNormals = false;
	OutCollisionData->bDeformableMesh = false; // Static voxel terrain: false enables Chaos static BVH optimization
	OutCollisionData->bFastCook = true;

	return true;
}

bool UVoxelCollisionComponent::GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	if (!CollisionData.IsValid() || CollisionData->IsEmpty())
	{
		return false;
	}

	OutTriMeshEstimates.VerticeCount = CollisionData->Vertices.Num();
	return true;
}

bool UVoxelCollisionComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return CollisionData.IsValid() && !CollisionData->IsEmpty();
}

UBodySetup* UVoxelCollisionComponent::GetBodySetup()
{
	return BodySetup;
}

FBoxSphereBounds UVoxelCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return LocalBounds.TransformBy(LocalToWorld);
}

void UVoxelCollisionComponent::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();
}

void UVoxelCollisionComponent::OnUnregister()
{
	ClearCollisionData();
	Super::OnUnregister();
}
