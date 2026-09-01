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
	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	EVoxelCollisionMode CollisionMode = EVoxelCollisionMode::Complex;

	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	bool bAsyncCooking = true;

	UPROPERTY(EditDefaultsOnly, Category = "Collision")
	FName CollisionProfileName = TEXT("BlockAll");
};
