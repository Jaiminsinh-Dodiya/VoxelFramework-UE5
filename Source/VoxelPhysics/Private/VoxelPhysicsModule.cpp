// VoxelPhysicsModule.cpp

#include "VoxelPhysicsModule.h"

DEFINE_LOG_CATEGORY(LogVoxelPhysics);

void FVoxelPhysicsModule::StartupModule()
{
	UE_LOG(LogVoxelPhysics, Log, TEXT("VoxelPhysics module started."));
}

void FVoxelPhysicsModule::ShutdownModule()
{
	UE_LOG(LogVoxelPhysics, Log, TEXT("VoxelPhysics module shut down."));
}

IMPLEMENT_MODULE(FVoxelPhysicsModule, VoxelPhysics);
