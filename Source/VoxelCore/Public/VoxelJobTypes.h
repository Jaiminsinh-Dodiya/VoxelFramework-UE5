// VoxelJobTypes.h
//
// Purpose:
//   Shared job-state model used by every async voxel operation (generation,
//   meshing, GPU upload, serialization). Defined once in VoxelCore so
//   VoxelGeneration, VoxelMeshing, VoxelRendering, and VoxelStreaming all
//   report status the same way without depending on each other.
//
// Responsibilities:
//   Plain state enum + a minimal handle struct. No scheduling logic here -
//   that belongs to VoxelRuntime's task wrappers and, later, VoxelStreaming.
//
// Thread ownership: value type, no shared mutable state.
// Dependencies: Core only.
//
// Design note (see Docs/ADR.md, "Scheduler cancellation stance"):
//   EVoxelJobState::Cancelled exists starting Phase 1 even though nothing
//   sets it yet. This is deliberate - it means Phase 5 streaming can add
//   real cancellation without changing this type or anything that already
//   switches on it (a new enum value later would force every switch
//   statement to be revisited; having it now costs nothing since Queued/
//   Running/Completed already need a default case).

#pragma once

#include "CoreMinimal.h"

enum class EVoxelJobState : uint8
{
	Queued,
	Running,
	Completed,
	Cancelled
};

/** Non-owning reference to an in-flight or finished voxel job. */
struct FVoxelJobHandle
{
	uint64 JobId = 0;

	bool IsValid() const { return JobId != 0; }

	bool operator==(const FVoxelJobHandle& Other) const { return JobId == Other.JobId; }
};

FORCEINLINE uint32 GetTypeHash(const FVoxelJobHandle& Handle)
{
	return GetTypeHash(Handle.JobId);
}
