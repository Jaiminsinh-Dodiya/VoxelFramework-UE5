// VoxelCoreModule.h
//
// Purpose:
//   Trivial module entry point for VoxelCore. Per ADR-001/002, VoxelCore
//   owns no systems (no thread pool, no settings) - that lives in
//   VoxelRuntime. This module exists only so VoxelCore's types/interfaces
//   are packaged as a proper UE module with its own log category.
//
// Responsibilities:
//   - Declare LogVoxelCore
//   - Nothing else. If you're about to add a member variable here, it
//     probably belongs in VoxelRuntime instead.
//
// Thread ownership: Game Thread (module load/unload only).
// Dependencies: Core, CoreUObject.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

VOXELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoxelCore, Log, All);

class FVoxelCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};
