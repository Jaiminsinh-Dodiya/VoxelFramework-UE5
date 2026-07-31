// VoxelStorage has no module-level singleton yet in Phase 1 - FVoxelChunkStore
// instances are owned by whoever needs one (tests, and later VoxelWorldSubsystem
// in Phase 5), not by the module itself.

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, VoxelStorage)
