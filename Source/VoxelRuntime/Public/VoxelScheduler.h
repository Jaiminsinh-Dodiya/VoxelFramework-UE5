// VoxelScheduler.h
//
// Purpose:
//   Thin, opinionated wrapper over UE::Tasks that gives every Voxel*
//   module a single, consistent way to submit background work with a
//   voxel-meaningful priority (distance-to-player driven), instead of each
//   module choosing UE::Tasks priorities ad hoc.
//
// Responsibilities:
//   - Translate EVoxelWorkPriority -> UE::Tasks::ETaskPriority
//   - Track job state (EVoxelJobState) per submitted job so callers and,
//     later, VoxelStreaming can query/cancel by handle
//   - Nothing else - actual scheduling, work-stealing, and thread counts
//     are the engine's job, not ours (ADR-002)
//
// Thread ownership:
//   Submit() safe from any thread. GetState() safe from any thread.
//   Completion callbacks run on whatever thread UE::Tasks chooses to
//   finish the task on - if a caller needs Game Thread affinity, it must
//   marshal that itself (e.g. via AsyncTask(ENamedThreads::GameThread, ...))
//   inside its own OnComplete lambda. VoxelStreaming's drain step is the
//   expected place that happens; VoxelScheduler does not enforce it.
//
// Dependencies: Core, VoxelCore (EVoxelWorkPriority, EVoxelJobState).
//
// Performance notes: job state map uses a critical section; expected
// contention is low (one insert per submit, one lookup per query/cancel-check).

#pragma once

#include "CoreMinimal.h"
#include "VoxelJobTypes.h"
#include "Tasks/Task.h"

class VOXELRUNTIME_API FVoxelScheduler
{
public:
	/**
	 * Submits work to run via UE::Tasks. OnComplete (if bound) runs on
	 * whichever thread the task completes on - see thread ownership note
	 * above regarding Game Thread marshaling.
	 */
	FVoxelJobHandle Submit(TFunction<void()> Work, EVoxelWorkPriority Priority, TFunction<void()> OnComplete = nullptr);

	/** Current state of a submitted job. Returns Cancelled if the handle is unknown/stale. */
	EVoxelJobState GetState(FVoxelJobHandle Handle) const;

	/**
	 * Marks a job as requested-for-cancellation. Per ADR (Phase 1-4), no
	 * caller invokes this yet and Submit()'d work does not check for it -
	 * the state transition exists so Phase 5 streaming can start checking
	 * it inside long-running work without changing this class's API.
	 */
	void RequestCancel(FVoxelJobHandle Handle);

private:
	static UE::Tasks::ETaskPriority ToTaskPriority(EVoxelWorkPriority Priority);

	mutable FCriticalSection StateLock;
	TMap<uint64, EVoxelJobState> JobStates;
	uint64 NextJobId = 1;
};
