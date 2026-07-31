// VoxelRuntimeModule.cpp

#include "VoxelRuntimeModule.h"
#include "VoxelScheduler.h"
#include "VoxelCoreModule.h"

void FVoxelRuntimeModule::StartupModule()
{
	Scheduler = MakeUnique<FVoxelScheduler>();
	UE_LOG(LogVoxelCore, Log, TEXT("VoxelRuntime started (scheduling via UE::Tasks per ADR-002)."));
}

void FVoxelRuntimeModule::ShutdownModule()
{
	// Dependent modules (Storage/Generation/Meshing/Rendering/Streaming) must
	// have flushed their own in-flight jobs during their own ShutdownModule
	// before this runs - UE's module dependency ordering guarantees they
	// shut down first since they all depend on VoxelRuntime.
	Scheduler.Reset();
}

FVoxelRuntimeModule& FVoxelRuntimeModule::Get()
{
	return FModuleManager::LoadModuleChecked<FVoxelRuntimeModule>("VoxelRuntime");
}

bool FVoxelRuntimeModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("VoxelRuntime");
}

FVoxelScheduler& FVoxelRuntimeModule::GetScheduler() const
{
	check(Scheduler.IsValid());
	return *Scheduler;
}

IMPLEMENT_MODULE(FVoxelRuntimeModule, VoxelRuntime)
