// VoxelScheduler.cpp

#include "VoxelScheduler.h"
#include "VoxelCoreModule.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelRuntime, Log, All);

FVoxelJobHandle FVoxelScheduler::Submit(TFunction<void()> Work, EVoxelWorkPriority Priority, TFunction<void()> OnComplete)
{
	uint64 JobId;
	{
		FScopeLock Lock(&StateLock);
		JobId = NextJobId++;
		JobStates.Add(JobId, EVoxelJobState::Queued);
	}

	ActiveTaskCount++;
	const FVoxelJobHandle Handle{ JobId };
	const UE::Tasks::ETaskPriority TaskPriority = ToTaskPriority(Priority);

	UE::Tasks::Launch(TEXT("VoxelJob"),
		[this, JobId, Work = MoveTemp(Work), OnComplete = MoveTemp(OnComplete)]()
		{
			bool bShouldRunWork = false;
			{
				FScopeLock Lock(&StateLock);
				if (EVoxelJobState* State = JobStates.Find(JobId))
				{
					if (*State != EVoxelJobState::Cancelled)
					{
						*State = EVoxelJobState::Running;
						bShouldRunWork = true;
					}
				}
			}

			if (bShouldRunWork && Work)
			{
				Work();
			}

			{
				FScopeLock Lock(&StateLock);
				if (EVoxelJobState* State = JobStates.Find(JobId))
				{
					if (*State != EVoxelJobState::Cancelled)
					{
						*State = EVoxelJobState::Completed;
					}
				}
				PruneJobStatesUnderLock();
			}

			// Terminal completion invariant: OnComplete is GUARANTEED to execute exactly once,
			// whether Work ran to completion, was cancelled before starting, or was cancelled mid-run.
			if (OnComplete)
			{
				OnComplete();
			}

			ActiveTaskCount--;
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
	// If handle is within historical bounds, it finished and was pruned.
	if (Handle.JobId > 0 && Handle.JobId < NextJobId)
	{
		return EVoxelJobState::Completed;
	}
	return EVoxelJobState::Cancelled;
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

bool FVoxelScheduler::WaitForAllTasks(double TimeoutSeconds)
{
	const double StartTime = FPlatformTime::Seconds();
	while (ActiveTaskCount.Load() > 0)
	{
		if (TimeoutSeconds > 0.0 && (FPlatformTime::Seconds() - StartTime) >= TimeoutSeconds)
		{
			UE_LOG(LogVoxelRuntime, Error,
				TEXT("FVoxelScheduler::WaitForAllTasks timed out after %.2fs with %d active tasks! Live storage must NOT be destroyed."),
				TimeoutSeconds, ActiveTaskCount.Load());
			return false;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return true;
}

int32 FVoxelScheduler::GetTrackedJobCount() const
{
	FScopeLock Lock(&StateLock);
	return JobStates.Num();
}

void FVoxelScheduler::PruneJobStatesUnderLock()
{
	if (JobStates.Num() <= MaxRetainedCompletedJobStates)
	{
		return;
	}

	const uint64 CutoffJobId = (NextJobId > static_cast<uint64>(MaxRetainedCompletedJobStates))
		? (NextJobId - static_cast<uint64>(MaxRetainedCompletedJobStates))
		: 0;

	if (CutoffJobId == 0)
	{
		return;
	}

	// Only prune completed or cancelled historical entries that are older than CutoffJobId.
	// Active/Queued/Running jobs are NEVER pruned.
	for (auto It = JobStates.CreateIterator(); It; ++It)
	{
		if (It.Key() < CutoffJobId)
		{
			if (It.Value() == EVoxelJobState::Completed || It.Value() == EVoxelJobState::Cancelled)
			{
				It.RemoveCurrent();
			}
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
