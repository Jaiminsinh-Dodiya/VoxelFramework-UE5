// VoxelPhysicsModule.h

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

VOXELPHYSICS_API DECLARE_LOG_CATEGORY_EXTERN(LogVoxelPhysics, Log, All);

class FVoxelPhysicsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FVoxelPhysicsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FVoxelPhysicsModule>("VoxelPhysics");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("VoxelPhysics");
	}
};
