// VoxelStreamingCorrectnessTests.cpp

#include "Misc/AutomationTest.h"
#include "VoxelCoreTypes.h"
#include "VoxelChunkStore.h"
#include "VoxelChunk.h"
#include "VoxelScheduler.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

// ============================================================================
// Test 1: Storage Worker Lease Lifecycle & Delayed Recycling
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStorageWorkerLeaseTest,
	"Voxel.Streaming.StorageWorkerLeaseLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStorageWorkerLeaseTest::RunTest(const FString& Parameters)
{
	FVoxelChunkStore Store(32);
	const FVoxelChunkCoordinate CoordA(1, 2, 3);
	const FVoxelChunkCoordinate CoordB(4, 5, 6);

	// 1. Create Chunk A
	const FVoxelChunkHandle HandleA = Store.CreateOrGetChunk(CoordA);
	TestTrue(TEXT("HandleA is valid"), HandleA.IsValid());
	TestEqual(TEXT("Initial loaded count is 1"), Store.GetLoadedChunkCount(), 1);

	// 2. Acquire worker lease on Chunk A
	const int32 SlotIndexA = Store.AcquireWorkerLease(CoordA);
	TestTrue(TEXT("SlotIndexA is valid"), SlotIndexA != INDEX_NONE);
	TestTrue(TEXT("Slot is busy"), Store.IsSlotBusy(SlotIndexA));

	// 3. Unload Chunk A while worker lease is active
	Store.RemoveChunk(CoordA);
	TestEqual(TEXT("Loaded count is 0 after unload"), Store.GetLoadedChunkCount(), 0);
	TestNull(TEXT("FindChunkByCoordinate returns nullptr after unload"), Store.FindChunkByCoordinate(CoordA));
	TestTrue(TEXT("Slot is still busy because worker holds lease"), Store.IsSlotBusy(SlotIndexA));

	// 4. Create Chunk B - must NOT reuse SlotIndexA because Chunk A's worker still holds lease!
	const FVoxelChunkHandle HandleB = Store.CreateOrGetChunk(CoordB);
	const int32 SlotIndexB = Store.AcquireWorkerLease(CoordB);
	TestTrue(TEXT("Chunk B got a different slot (no premature recycling)"), SlotIndexB != SlotIndexA);
	Store.ReleaseWorkerLease(SlotIndexB);

	// 5. Worker finishes and releases lease on Chunk A
	Store.ReleaseWorkerLease(SlotIndexA);
	TestFalse(TEXT("Slot is no longer busy after release"), Store.IsSlotBusy(SlotIndexA));

	// 6. Now create Chunk C - can safely reuse SlotIndexA
	const FVoxelChunkCoordinate CoordC(7, 8, 9);
	const FVoxelChunkHandle HandleC = Store.CreateOrGetChunk(CoordC);
	const int32 SlotIndexC = Store.AcquireWorkerLease(CoordC);
	TestEqual(TEXT("Chunk C safely recycled freed slot"), SlotIndexC, SlotIndexA);
	Store.ReleaseWorkerLease(SlotIndexC);

	return true;
}

// ============================================================================
// Test 2: State Machine Enum Integrity
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelChunkStateMachineTest,
	"Voxel.Streaming.StateMachineTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelChunkStateMachineTest::RunTest(const FString& Parameters)
{
	// Verify state enum ordering and default values
	TestEqual(TEXT("Unloaded is 0"), static_cast<uint8>(EVoxelChunkState::Unloaded), 0);
	TestEqual(TEXT("Queued is 1"), static_cast<uint8>(EVoxelChunkState::Queued), 1);
	TestEqual(TEXT("Generating is 2"), static_cast<uint8>(EVoxelChunkState::Generating), 2);
	TestEqual(TEXT("Meshing is 3"), static_cast<uint8>(EVoxelChunkState::Meshing), 3);
	TestEqual(TEXT("PendingFinalize is 4"), static_cast<uint8>(EVoxelChunkState::PendingFinalize), 4);
	TestEqual(TEXT("Ready is 5"), static_cast<uint8>(EVoxelChunkState::Ready), 5);
	TestEqual(TEXT("Unloading is 6"), static_cast<uint8>(EVoxelChunkState::Unloading), 6);

	return true;
}

// ============================================================================
// Test 3: Distance Priority Mapping
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelDistancePriorityTest,
	"Voxel.Streaming.DistancePriorityMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelDistancePriorityTest::RunTest(const FString& Parameters)
{
	const int32 Sim = 4;
	const int32 Ren = 8;
	const int32 Gen = 10;

	auto MapPriority = [Sim, Ren, Gen](int32 Dist) -> EVoxelWorkPriority
	{
		if (Dist <= Sim) return EVoxelWorkPriority::Critical;
		if (Dist <= Ren) return EVoxelWorkPriority::High;
		if (Dist <= Gen) return EVoxelWorkPriority::Normal;
		return EVoxelWorkPriority::Low;
	};

	TestEqual(TEXT("Dist 0 is Critical"), MapPriority(0), EVoxelWorkPriority::Critical);
	TestEqual(TEXT("Dist 4 is Critical (boundary)"), MapPriority(4), EVoxelWorkPriority::Critical);
	TestEqual(TEXT("Dist 5 is High"), MapPriority(5), EVoxelWorkPriority::High);
	TestEqual(TEXT("Dist 8 is High (boundary)"), MapPriority(8), EVoxelWorkPriority::High);
	TestEqual(TEXT("Dist 9 is Normal"), MapPriority(9), EVoxelWorkPriority::Normal);
	TestEqual(TEXT("Dist 10 is Normal (boundary)"), MapPriority(10), EVoxelWorkPriority::Normal);
	TestEqual(TEXT("Dist 11 is Low (distant background)"), MapPriority(11), EVoxelWorkPriority::Low);

	return true;
}

// ============================================================================
// Test 4: Scheduler Terminal Completion & Cancellation Guarantee (Phase 6.4.1)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSchedulerTerminalCompletionTest,
	"Voxel.Streaming.SchedulerTerminalCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSchedulerTerminalCompletionTest::RunTest(const FString& Parameters)
{
	FVoxelScheduler Scheduler;

	// Case 1: Cancel while Queued -> Work is skipped, but OnComplete MUST run exactly once
	{
		FEvent* BlockEvent = FPlatformProcess::GetSynchEventFromPool();
		bool bWorkRan = false;
		bool bOnCompleteRan = false;

		const FVoxelJobHandle Handle = Scheduler.Submit(
			[BlockEvent, &bWorkRan]()
			{
				bWorkRan = true;
				BlockEvent->Wait();
			},
			EVoxelWorkPriority::Low,
			[&bOnCompleteRan]()
			{
				bOnCompleteRan = true;
			});

		// Cancel immediately while queued
		Scheduler.RequestCancel(Handle);
		FPlatformProcess::Sleep(0.05f);

		TestEqual(TEXT("Case 1: State is Cancelled"), Scheduler.GetState(Handle), EVoxelJobState::Cancelled);
		TestFalse(TEXT("Case 1: Work was skipped"), bWorkRan);
		TestTrue(TEXT("Case 1: OnComplete executed despite queued cancellation"), bOnCompleteRan);

		BlockEvent->Trigger();
		FPlatformProcess::ReturnSynchEventToPool(BlockEvent);
	}

	// Case 2: Duplicate cancel -> Idempotent
	{
		const FVoxelJobHandle Handle = Scheduler.Submit([]() {}, EVoxelWorkPriority::Low);
		Scheduler.RequestCancel(Handle);
		Scheduler.RequestCancel(Handle); // Second cancel call
		TestEqual(TEXT("Case 2: State remains Cancelled"), Scheduler.GetState(Handle), EVoxelJobState::Cancelled);
	}

	// Case 3: Cancel after completion -> State remains Completed
	{
		bool bOnCompleteRan = false;
		const FVoxelJobHandle Handle = Scheduler.Submit(
			[]() {},
			EVoxelWorkPriority::Critical,
			[&bOnCompleteRan]() { bOnCompleteRan = true; });

		// Wait for completion
		Scheduler.WaitForAllTasks(1.0);
		TestEqual(TEXT("Case 3: State is Completed"), Scheduler.GetState(Handle), EVoxelJobState::Completed);

		Scheduler.RequestCancel(Handle); // Cancel after finish
		TestEqual(TEXT("Case 3: State remains Completed after cancel"), Scheduler.GetState(Handle), EVoxelJobState::Completed);
		TestTrue(TEXT("Case 3: OnComplete executed"), bOnCompleteRan);
	}

	return true;
}

// ============================================================================
// Test 5: Neighbor Lifetime & Worker Lease Retention during Unload (Phase 6.4.2)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelNeighborLifetimeSafetyTest,
	"Voxel.Streaming.NeighborLifetimeSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelNeighborLifetimeSafetyTest::RunTest(const FString& Parameters)
{
	FVoxelChunkStore Store(32);
	const FVoxelChunkCoordinate TargetCoord(0, 0, 0);
	const FVoxelChunkCoordinate NeighborCoord(1, 0, 0);

	// 1. Create Target and Neighbor chunks
	Store.CreateOrGetChunk(TargetCoord);
	Store.CreateOrGetChunk(NeighborCoord);

	// 2. Mesher leases Target and Neighbor
	const int32 TargetSlot = Store.AcquireWorkerLease(TargetCoord);
	const int32 NeighborSlot = Store.AcquireWorkerLease(NeighborCoord);
	TestTrue(TEXT("Target slot leased"), TargetSlot != INDEX_NONE);
	TestTrue(TEXT("Neighbor slot leased"), NeighborSlot != INDEX_NONE);

	// 3. Neighbor is unloaded while Target mesher worker is still active
	Store.RemoveChunk(NeighborCoord);
	TestNull(TEXT("Neighbor chunk lookups return nullptr"), Store.FindChunkByCoordinate(NeighborCoord));
	TestTrue(TEXT("Neighbor slot memory is preserved because worker holds lease"), Store.IsSlotBusy(NeighborSlot));

	// 4. A new chunk is created - must NOT steal Neighbor's memory slot!
	const FVoxelChunkCoordinate NewCoord(9, 9, 9);
	const FVoxelChunkHandle NewHandle = Store.CreateOrGetChunk(NewCoord);
	const int32 NewSlot = Store.AcquireWorkerLease(NewCoord);
	TestTrue(TEXT("New chunk allocated a distinct slot"), NewSlot != NeighborSlot);
	Store.ReleaseWorkerLease(NewSlot);

	// 5. Mesher finishes and releases both leases
	Store.ReleaseWorkerLease(TargetSlot);
	Store.ReleaseWorkerLease(NeighborSlot);
	TestFalse(TEXT("Neighbor slot is no longer busy"), Store.IsSlotBusy(NeighborSlot));

	// 6. Next chunk creation can now safely recycle NeighborSlot
	const FVoxelChunkCoordinate RecycledCoord(10, 10, 10);
	Store.CreateOrGetChunk(RecycledCoord);
	const int32 RecycledSlot = Store.AcquireWorkerLease(RecycledCoord);
	TestEqual(TEXT("Slot safely recycled once all leases released"), RecycledSlot, NeighborSlot);
	Store.ReleaseWorkerLease(RecycledSlot);

	return true;
}

// ============================================================================
// Test 6: Bounded Scheduler Job History Retention (Phase 6.4.5)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelSchedulerBoundedHistoryTest,
	"Voxel.Streaming.SchedulerBoundedHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelSchedulerBoundedHistoryTest::RunTest(const FString& Parameters)
{
	FVoxelScheduler Scheduler;
	const int32 BoundedLimit = 512;
	Scheduler.SetMaxRetainedJobStates(BoundedLimit);

	// Submit 2,000 lightweight jobs
	const int32 JobCount = 2000;
	for (int32 i = 0; i < JobCount; ++i)
	{
		Scheduler.Submit([]() {}, EVoxelWorkPriority::Normal);
	}

	// Wait for all jobs to complete
	const bool bAllDone = Scheduler.WaitForAllTasks(5.0);
	TestTrue(TEXT("All 2,000 jobs completed"), bAllDone);

	// Verify tracked job count is bounded to approximately BoundedLimit
	const int32 TrackedCount = Scheduler.GetTrackedJobCount();
	TestTrue(TEXT("JobStates map is bounded"), TrackedCount <= BoundedLimit + 10);

	return true;
}

// ============================================================================
// Test 7: Long-Run Streaming Stress & Slot Stability (Phase 6.4.7)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingLongRunStressTest,
	"Voxel.Streaming.LongRunStress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingLongRunStressTest::RunTest(const FString& Parameters)
{
	FVoxelChunkStore Store(32);
	FVoxelScheduler Scheduler;
	Scheduler.SetMaxRetainedJobStates(256);

	// Rapidly churn through 1,000 chunks with concurrent worker leasing and removals
	constexpr int32 ChurnIterations = 1000;
	for (int32 i = 0; i < ChurnIterations; ++i)
	{
		const FVoxelChunkCoordinate Coord(i % 16, (i / 16) % 16, i % 4);
		Store.CreateOrGetChunk(Coord);
		const int32 Slot = Store.AcquireWorkerLease(Coord);

		Scheduler.Submit(
			[]() { /* Simulating short background workload */ },
			EVoxelWorkPriority::Normal,
			[&Store, Slot]()
			{
				if (Slot != INDEX_NONE)
				{
					Store.ReleaseWorkerLease(Slot);
				}
			});

		// Every 4 iterations, remove older coordinates
		if ((i % 4) == 0 && i >= 16)
		{
			const FVoxelChunkCoordinate OldCoord((i - 16) % 16, ((i - 16) / 16) % 16, (i - 16) % 4);
			Store.RemoveChunk(OldCoord);
		}
	}

	// Wait for all asynchronous tasks to settle
	const bool bSettled = Scheduler.WaitForAllTasks(5.0);
	TestTrue(TEXT("Stress tasks settled within timeout"), bSettled);

	// Verify total slots created remained bounded by active volume
	TestTrue(TEXT("Store total slot count remained bounded"), Store.GetTotalSlotCount() <= 256);

	return true;
}
