// VoxelMeshingService.h
//
// Purpose:
//   Thin convenience wrapper so callers don't have to hand-write the
//   FVoxelScheduler::Submit boilerplate every time they want a mesh built
//   off the Game Thread. Does NOT introduce a new scheduling abstraction -
//   it is a one-line pass-through to FVoxelRuntimeModule::Get().GetScheduler().
//
// Responsibilities: dispatch FVoxelMesher::GenerateMesh via the existing
//   scheduler. Nothing else.
//
// Thread ownership: RequestMeshAsync is safe to call from the Game Thread
//   (or any thread FVoxelScheduler::Submit already supports). OnComplete
//   runs on whichever thread the job completes on - same caveat as
//   FVoxelScheduler::Submit itself (see VoxelRuntime/VoxelScheduler.h) -
//   callers needing Game Thread affinity must marshal it themselves.
//
// IMPORTANT lifetime caveat: this takes Chunk BY POINTER and captures it
// into the dispatched work. The caller is responsible for guaranteeing the
// FVoxelChunk outlives the job - there is no owning/ref-counted chunk
// lifetime system yet (that's a VoxelWorldSubsystem/VoxelStreaming concern,
// not built yet - see Docs/TODO.md). Do not use this helper with a chunk
// that might be pooled/reused before the job completes until that exists.
//
// Dependencies: VoxelRuntime (FVoxelScheduler), this module's VoxelMesher.

#pragma once

#include "CoreMinimal.h"
#include "VoxelJobTypes.h"

class FVoxelChunk;
class UVoxelBlockRegistry;
struct FVoxelMeshData;

namespace VoxelMeshingService
{
	/** See lifetime caveat above regarding Chunk's pointer validity for the job's duration. */
	VOXELMESHING_API FVoxelJobHandle RequestMeshAsync(
		const FVoxelChunk* Chunk,
		const UVoxelBlockRegistry* BlockRegistry,
		TFunction<void(FVoxelMeshData&&)> OnComplete);
}
