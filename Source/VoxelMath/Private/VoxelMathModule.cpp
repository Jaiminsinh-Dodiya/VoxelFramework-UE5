// VoxelMathModule.cpp
//
// VoxelMath has no module-level state (see VoxelNoise.h - everything is
// pure functions), so it uses the engine's default module implementation
// rather than a custom IModuleInterface subclass.

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, VoxelMath)
