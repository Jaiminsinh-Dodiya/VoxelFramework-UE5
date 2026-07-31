// VoxelScheduler.cpp

#include "VoxelScheduler.h"
#include "VoxelCoreModule.h"

FVoxelJobHandle FVoxelScheduler::Submit(TFunction<void()> Work, EVoxelWorkPriority Priority, TFunction<void()> OnComplete)
{
	uint64 JobId;
	{
		FScopeLock Lock(&StateLock);
		JobId = NextJobId++;
		JobStates.Add(JobId, EVoxelJobState::Queued);
	}

	const FVoxelJobHandle Handle{ JobId };
	const UE::Tasks::ETaskPriority TaskPriority = ToTaskPriority(Priority);

	// Capture 'this' is safe: FVoxelScheduler lives for the lifetime of the
	// VoxelRuntime module, and Voxel* modules are expected to flush/cancel
	// their own in-flight work during their own ShutdownModule before
	// VoxelRuntime shuts down (module dependency order guarantees this).
	UE::Tasks::Launch(TEXT("VoxelJob"),
		[this, JobId, Work = MoveTemp(Work), OnComplete = MoveTemp(OnComplete)]()
		{
			{
				FScopeLock Lock(&StateLock);
				if (EVoxelJobState* State = JobStates.Find(JobId))
				{
					if (*State == EVoxelJobState::Cancelled)
					{
						return; // cancellation requested before it started running
					}
					*State = EVoxelJobState::Running;
				}
			}

			if (Work)
			{
				Work();
			}

			{
				FScopeLock Lock(&StateLock);
				if (EVoxelJobState* State = JobStates.Find(JobId))
				{
					// Don't stomp a Cancelled state set mid-run (no one sets this yet,
					// but this keeps the transition safe once Phase 5 wires it up).
					if (*State != EVoxelJobState::Cancelled)
					{
						*State = EVoxelJobState::Completed;
					}
				}
			}

			if (OnComplete)
			{
				OnComplete();
			}
		},
		TaskPriority);

	return Handle;
}

EVoxelJobState FVoxelScheduler::GetState(FVoxelJobHandle Handle) const
{
	FScopeLock Lock(&StateLock);
	if (const EVoxelJobState* State = JobStates.Find(Handle.JobId))
	{
		return *State;
	}
	return EVoxelJobState::Cancelled; // unknown handle treated as terminal/cancelled, not an error state
}

void FVoxelScheduler::RequestCancel(FVoxelJobHandle Handle)
{
	FScopeLock Lock(&StateLock);
	if (EVoxelJobState* State = JobStates.Find(Handle.JobId))
	{
		if (*State == EVoxelJobState::Queued || *State == EVoxelJobState::Running)
		{
			*State = EVoxelJobState::Cancelled;
		}
	}
}

UE::Tasks::ETaskPriority FVoxelScheduler::ToTaskPriority(EVoxelWorkPriority Priority)
{
	switch (Priority)
	{
	case EVoxelWorkPriority::Critical: return UE::Tasks::ETaskPriority::High;
	case EVoxelWorkPriority::High:     return UE::Tasks::ETaskPriority::High;
	case EVoxelWorkPriority::Normal:   return UE::Tasks::ETaskPriority::Normal;
	case EVoxelWorkPriority::Low:
	default:                           return UE::Tasks::ETaskPriority::BackgroundLow;
	}
}
