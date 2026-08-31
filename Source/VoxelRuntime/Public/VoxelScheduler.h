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
#include "HAL/PlatformTime.h"

class VOXELRUNTIME_API FVoxelScheduler
{
public:
	/**
	 * Submits work to run via UE::Tasks.
	 *
	 * Invariant: OnComplete (if bound) is GUARANTEED to execute exactly once when the job
	 * reaches its terminal state, whether Work ran to completion, was cancelled before starting,
	 * or was cancelled while running.
	 */
	FVoxelJobHandle Submit(TFunction<void()> Work, EVoxelWorkPriority Priority, TFunction<void()> OnComplete = nullptr);

	/** Current state of a submitted job. Returns Cancelled/Completed if the handle is unknown or pruned. */
	EVoxelJobState GetState(FVoxelJobHandle Handle) const;

	/** Marks a job as requested-for-cancellation. Idempotent and thread-safe. */
	void RequestCancel(FVoxelJobHandle Handle);

	/** Number of submitted tasks currently active (either Queued or Running). */
	int32 GetActiveTaskCount() const { return ActiveTaskCount.Load(); }

	/**
	 * Blocks until all active worker tasks have finished execution.
	 * TimeoutSeconds is a diagnostic timeout. If it expires while tasks are still active,
	 * returns false without permitting unsafe teardown.
	 */
	bool WaitForAllTasks(double TimeoutSeconds = 5.0);

	/** Configures the bounded retention limit for historical completed/cancelled job states. */
	void SetMaxRetainedJobStates(int32 MaxStates) { MaxRetainedCompletedJobStates = FMath::Max(256, MaxStates); }
	int32 GetMaxRetainedJobStates() const { return MaxRetainedCompletedJobStates; }
	int32 GetTrackedJobCount() const;

private:
	static UE::Tasks::ETaskPriority ToTaskPriority(EVoxelWorkPriority Priority);
	void PruneJobStatesUnderLock();

	mutable FCriticalSection StateLock;
	TMap<uint64, EVoxelJobState> JobStates;
	uint64 NextJobId = 1;
	int32 MaxRetainedCompletedJobStates = 8192;
	TAtomic<int32> ActiveTaskCount{ 0 };
};
