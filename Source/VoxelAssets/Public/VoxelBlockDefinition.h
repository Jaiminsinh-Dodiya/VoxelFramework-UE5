// VoxelBlockDefinition.h
//
// Purpose:
//   Data-asset definition of a single block type. FVoxelBlockId (VoxelCore)
//   is just a uint16 - this asset is what gives that number meaning
//   (material, solidity, display name) without any engine code change per
//   new block, per the original spec's "adding a biome/block should
//   require no engine code" requirement.
//
// Responsibilities: pure data. No generation/rendering logic lives here -
//   VoxelMeshing reads MaterialId/bIsSolid to decide face culling and
//   material assignment, it does not know anything else about a block.
// Thread ownership: UObject/data asset - Game Thread load, safe to read
//   (not write) from worker threads once loaded, same as any UDataAsset.
// Dependencies: Core, CoreUObject, Engine, VoxelCore.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VoxelCoreTypes.h"
#include "VoxelBlockDefinition.generated.h"

UCLASS(BlueprintType)
class VOXELASSETS_API UVoxelBlockDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Stable ID this definition answers for. Set once, never reused for a different block after ship (breaks save compatibility). */
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	int32 BlockId = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Block")
	FText DisplayName;

	/** Whether this block occupies its full cell for face-culling purposes (air, water, foliage = false). */
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	bool bIsSolid = true;

	/** Whether this block should generate collision. Some solids (foliage, decorative) may still be non-solid for collision. */
	UPROPERTY(EditDefaultsOnly, Category = "Block")
	bool bGeneratesCollision = true;

	/** Index into the shared texture atlas / material layer used by VoxelMeshing when assigning UVs. */
	UPROPERTY(EditDefaultsOnly, Category = "Rendering")
	int32 MaterialLayerIndex = 0;

	/** Optional per-block tint applied at vertex-color level (cheap variation without new materials). */
	UPROPERTY(EditDefaultsOnly, Category = "Rendering")
	FLinearColor VertexTint = FLinearColor::White;
};
