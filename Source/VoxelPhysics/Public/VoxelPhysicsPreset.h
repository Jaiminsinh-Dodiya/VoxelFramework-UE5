// VoxelPhysicsPreset.h
//
// Purpose:
//   Data asset for configuring voxel physics behavior per-world
//   Owned by VoxelPhysics module (physics-specific types stay in physics module)
//   Referenced from UVoxelWorldDefinition via TSoftObjectPtr
//   Consumed at initialization, not at runtime by workers

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelPhysicsTypes.h"
#include "VoxelPhysicsPreset.generated.h"

UCLASS(BlueprintType)
class VOXELPHYSICS_API UVoxelPhysicsPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Collision", meta = (ToolTip = "Collision geometry fidelity. 'Complex' generates full triangle mesh collision matching the voxel terrain surface. This is the only mode currently implemented."))
	EVoxelCollisionMode CollisionMode = EVoxelCollisionMode::Complex;

	UPROPERTY(EditDefaultsOnly, Category = "Collision", meta = (ToolTip = "If true, collision triangle meshes are cooked asynchronously using Chaos physics (off the Game Thread). Disabling this forces synchronous cooking which will cause frame hitches. Recommended: true."))
	bool bAsyncCooking = true;

	UPROPERTY(EditDefaultsOnly, Category = "Collision", meta = (ToolTip = "Unreal collision profile name applied to voxel collision components. Default 'BlockAll' blocks all movement and traces. Use custom profiles for selective channel filtering."))
	FName CollisionProfileName = TEXT("BlockAll");

};
