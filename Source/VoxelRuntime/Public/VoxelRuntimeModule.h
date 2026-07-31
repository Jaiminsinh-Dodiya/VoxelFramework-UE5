// VoxelRuntimeModule.h
//
// Purpose:
//   Owns process-wide voxel framework systems: the task scheduler wrapper
//   (ADR-002: UE::Tasks, not a custom thread pool) and access to global
//   settings. This is the module every Voxel* system-owning module
//   (Storage, Generation, Meshing, Rendering, Streaming) depends on for
//   "how do I run background work."
//
// Responsibilities:
//   - Module startup/shutdown lifecycle
//   - Provide FVoxelScheduler, a thin priority-aware wrapper over UE::Tasks
//   - Provide static accessor so other modules never touch UE::Tasks
//     directly with ad-hoc priority choices scattered across the codebase
//
// Thread ownership:
//   StartupModule/ShutdownModule run on the Game Thread. FVoxelScheduler's
//   public methods are safe to call from any thread once the module has
//   started (matches UE::Tasks' own thread-safety guarantees).
//
// Dependencies: Core, CoreUObject, Engine, VoxelCore, Projects.
//
// Performance notes: no per-frame cost here beyond what callers explicitly
// submit through FVoxelScheduler.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FVoxelScheduler;

class VOXELRUNTIME_API FVoxelRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FVoxelRuntimeModule& Get();
	static bool IsAvailable();

	FVoxelScheduler& GetScheduler() const;

private:
	TUniquePtr<FVoxelScheduler> Scheduler;
};
