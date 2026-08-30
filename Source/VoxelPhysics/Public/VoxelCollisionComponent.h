// VoxelCollisionComponent.h
//
// Purpose:
//   Production collision component providing Unreal/Chaos physics representation
//   for a chunk from an immutable FVoxelCollisionData snapshot.
//
// Responsibilities:
//   - Implements IInterface_CollisionDataProvider to supply vertex/index arrays to Chaos
//   - Manages UBodySetup lifecycle and async physics mesh cooking
//   - Registers with Chaos FPhysScene via RecreatePhysicsState()
//   - Completely independent from UVoxelMeshComponent and rendering
//
// Thread ownership:
//   Component setup, SetCollisionData, and RecreatePhysicsState run on Game Thread.
//   GetPhysicsTriMeshData is called by Chaos on async cook worker threads.
//
// Dependencies: Engine (UPrimitiveComponent, UBodySetup), VoxelPhysics (FVoxelCollisionData).

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "VoxelCollisionData.h"
#include "VoxelPhysicsTypes.h"
#include "VoxelCollisionComponent.generated.h"

class UBodySetup;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnVoxelCollisionCookFinished, UVoxelCollisionComponent*, bool /*bSuccess*/, uint32 /*Revision*/);

UCLASS(ClassGroup = VoxelPhysics, meta = (BlueprintSpawnableComponent))
class VOXELPHYSICS_API UVoxelCollisionComponent : public UPrimitiveComponent, public IInterface_CollisionDataProvider
{
	GENERATED_BODY()

public:
	UVoxelCollisionComponent();

	/** Fired on Game Thread when an async physics cook completes or fails. */
	FOnVoxelCollisionCookFinished OnCollisionCookFinished;

	/**
	 * Installs a new collision data snapshot and initiates Chaos collision cooking.
	 *
	 * @param InCollisionData   Plain CPU snapshot of vertices, indices, and bounds.
	 * @param bAsyncCook        If true, Chaos cooks collision on background threads without stalling Game Thread.
	 */
	void SetCollisionData(FVoxelCollisionData&& InCollisionData, bool bAsyncCook = true);

	/** Clears collision geometry and tears down active physics state. */
	void ClearCollisionData();

	/** True if component currently has active, registered Chaos collision geometry. */
	bool HasActiveCollision() const;

	/** Current revision of collision data installed or in-flight. */
	uint32 GetCurrentCollisionRevision() const { return CurrentRevision; }

	//~ Begin IInterface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override { return false; }
	//~ End IInterface_CollisionDataProvider Interface

	//~ Begin UPrimitiveComponent Interface
	virtual UBodySetup* GetBodySetup() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void OnDestroyPhysicsState() override;
	virtual void OnUnregister() override;
	//~ End UPrimitiveComponent Interface

private:
	void FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup, uint32 CookedRevision);
	UBodySetup* CreateBodySetupHelper();

	/** Immutable collision geometry snapshot read by Chaos during cooking. */
	TSharedPtr<FVoxelCollisionData, ESPMode::ThreadSafe> CollisionData;

	/** Cooked Chaos physics body setup. */
	UPROPERTY(Transient)
	TObjectPtr<UBodySetup> BodySetup;

	/** Pending in-flight async body setups. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBodySetup>> PendingAsyncBodySetups;

	uint32 CurrentRevision = 0;
	FBoxSphereBounds LocalBounds;
};
