// VoxelPhysicsTypes.h
//
// Purpose:
//   Core enums and basic types for the VoxelPhysics module.
//
// Thread ownership: N/A (plain enums/value types).
// Dependencies: VoxelCore only.

#pragma once

#include "CoreMinimal.h"

/** Mode used to generate collision geometry for a chunk. */
UENUM(BlueprintType)
enum class EVoxelCollisionMode : uint8
{
	None,     // No collision geometry generated
	Complex   // Full per-poly / triangle mesh collision
};

/** Lifecycle state of a chunk's physical collision representation. */
UENUM(BlueprintType)
enum class EVoxelCollisionState : uint8
{
	NotRequired,  // Outside collision simulation distance
	Queued,       // Scheduled for worker-thread geometry building
	Building,     // Worker thread actively building vertex/index snapshot
	Cooking,      // Chaos physics engine actively cooking UBodySetup
	Ready,        // Collision active and registered in Chaos physics scene
	Unloading,    // Collision being destroyed / unlinked
	Failed        // Error during build or cook
};
