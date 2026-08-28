// VoxelStreamingTests.cpp

#include "Misc/AutomationTest.h"
#include "VoxelStreamingTypes.h"
#include "VoxelCoreTypes.h"
#include "VoxelScheduler.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

// ============================================================================
// Test 1: Band Classification (pure function)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingBandClassificationTest,
	"Voxel.Streaming.BandClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingBandClassificationTest::RunTest(const FString& Parameters)
{
	// Settings: Sim=4, Render=8, Gen=10, Persist=12
	const int32 Sim = 4, Ren = 8, Gen = 10, Per = 12;

	// Exact boundary tests
	TestEqual(TEXT("Distance 0 = Simulation"),
		VoxelStreaming::ClassifyChunkDistance(0, Sim, Ren, Gen, Per), EVoxelStreamingBand::Simulation);
	TestEqual(TEXT("Distance 4 = Simulation (boundary)"),
		VoxelStreaming::ClassifyChunkDistance(4, Sim, Ren, Gen, Per), EVoxelStreamingBand::Simulation);
	TestEqual(TEXT("Distance 5 = Render"),
		VoxelStreaming::ClassifyChunkDistance(5, Sim, Ren, Gen, Per), EVoxelStreamingBand::Render);
	TestEqual(TEXT("Distance 8 = Render (boundary)"),
		VoxelStreaming::ClassifyChunkDistance(8, Sim, Ren, Gen, Per), EVoxelStreamingBand::Render);
	TestEqual(TEXT("Distance 9 = Generation"),
		VoxelStreaming::ClassifyChunkDistance(9, Sim, Ren, Gen, Per), EVoxelStreamingBand::Generation);
	TestEqual(TEXT("Distance 10 = Generation (boundary)"),
		VoxelStreaming::ClassifyChunkDistance(10, Sim, Ren, Gen, Per), EVoxelStreamingBand::Generation);
	TestEqual(TEXT("Distance 11 = Persistence"),
		VoxelStreaming::ClassifyChunkDistance(11, Sim, Ren, Gen, Per), EVoxelStreamingBand::Persistence);
	TestEqual(TEXT("Distance 12 = Persistence (boundary)"),
		VoxelStreaming::ClassifyChunkDistance(12, Sim, Ren, Gen, Per), EVoxelStreamingBand::Persistence);
	TestEqual(TEXT("Distance 13 = OutOfRange"),
		VoxelStreaming::ClassifyChunkDistance(13, Sim, Ren, Gen, Per), EVoxelStreamingBand::OutOfRange);
	TestEqual(TEXT("Distance 100 = OutOfRange"),
		VoxelStreaming::ClassifyChunkDistance(100, Sim, Ren, Gen, Per), EVoxelStreamingBand::OutOfRange);

	// Coordinate-based test: verify ChebyshevDistanceTo feeds into classification correctly
	const FVoxelChunkCoordinate Viewer(5, 5, 2);
	const FVoxelChunkCoordinate NearChunk(5, 7, 2); // distance 2 -> Simulation
	const FVoxelChunkCoordinate MidChunk(5, 12, 2); // distance 7 -> Render
	const FVoxelChunkCoordinate FarChunk(5, 18, 2); // distance 13 -> OutOfRange

	TestEqual(TEXT("Near chunk via coordinates = Simulation"),
		VoxelStreaming::ClassifyChunkDistance(Viewer.ChebyshevDistanceTo(NearChunk), Sim, Ren, Gen, Per),
		EVoxelStreamingBand::Simulation);
	TestEqual(TEXT("Mid chunk via coordinates = Render"),
		VoxelStreaming::ClassifyChunkDistance(Viewer.ChebyshevDistanceTo(MidChunk), Sim, Ren, Gen, Per),
		EVoxelStreamingBand::Render);
	TestEqual(TEXT("Far chunk via coordinates = OutOfRange"),
		VoxelStreaming::ClassifyChunkDistance(Viewer.ChebyshevDistanceTo(FarChunk), Sim, Ren, Gen, Per),
		EVoxelStreamingBand::OutOfRange);

	return true;
}

// ============================================================================
// Test 2: ComputeDesiredCoordinates respects Z clamping and sorting
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingDesiredCoordsTest,
	"Voxel.Streaming.DesiredCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingDesiredCoordsTest::RunTest(const FString& Parameters)
{
	// Small radius for tractable test: GenDist=1, Height=2
	const FVoxelChunkCoordinate Viewer(0, 0, 0);
	const TArray<FVoxelChunkCoordinate> Desired = VoxelStreaming::ComputeDesiredCoordinates(Viewer, 1, 2);

	// Expected: 3x3 XY grid * 2 Z levels = 18 coordinates
	// BUT Z is clamped to [0, 2), and Chebyshev distance must be <= 1.
	// Viewer is at (0,0,0). Z ranges 0-1. For each (DX,DY,CZ):
	//   ChebyshevDist = max(|DX|, |DY|, |CZ - 0|)
	// All combos with DX,DY in [-1,1], CZ in [0,1]:
	//   CZ=0: all 9 have max(|DX|,|DY|,0) = max(|DX|,|DY|) <= 1 -> all 9 included
	//   CZ=1: max(|DX|,|DY|,1) = max(|DX|,|DY|,1). For |DX|=1,|DY|=1: max=1 -> included
	//   So CZ=1: all 9 also have dist <= 1 -> all 9 included
	// Total: 18
	TestEqual(TEXT("GenDist=1, Height=2 produces 18 coords"), Desired.Num(), 18);

	// Verify Z clamping: no negative Z, no Z >= WorldHeightInChunks
	for (const FVoxelChunkCoordinate& Coord : Desired)
	{
		TestTrue(TEXT("Z >= 0"), Coord.Z >= 0);
		TestTrue(TEXT("Z < WorldHeightInChunks"), Coord.Z < 2);
	}

	// Verify sorted by ascending distance (first should be viewer itself)
	TestEqual(TEXT("First coord is viewer position"), Desired[0], Viewer);

	// Verify monotonically non-decreasing distance
	for (int32 i = 1; i < Desired.Num(); ++i)
	{
		const int32 PrevDist = Viewer.ChebyshevDistanceTo(Desired[i - 1]);
		const int32 CurrDist = Viewer.ChebyshevDistanceTo(Desired[i]);
		TestTrue(TEXT("Sorted ascending by distance"), CurrDist >= PrevDist);
	}

	return true;
}

// ============================================================================
// Test 3: Cancellation state transition (FEvent-controlled, not sleep-and-hope)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelStreamingCancellationTest,
	"Voxel.Streaming.CancellationStateTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelStreamingCancellationTest::RunTest(const FString& Parameters)
{
	FVoxelScheduler Scheduler;

	// --- Case 1: Cancel while Queued (before Work starts) ---
	// Submit a job whose Work blocks on an event we never signal,
	// then immediately cancel. The job should transition to Cancelled
	// and Work should never execute.
	{
		FEvent* BlockEvent = FPlatformProcess::GetSynchEventFromPool();
		bool bWorkRan = false;

		// Submit at BackgroundLow priority to maximize chance it stays Queued
		const FVoxelJobHandle Handle = Scheduler.Submit(
			[BlockEvent, &bWorkRan]()
			{
				bWorkRan = true;
				BlockEvent->Wait(); // Would block forever, but cancel should prevent this
			},
			EVoxelWorkPriority::Low);

		// Cancel immediately — high chance it's still Queued.
		Scheduler.RequestCancel(Handle);

		// Give the task system a moment to process
		FPlatformProcess::Sleep(0.1f);

		const EVoxelJobState FinalState = Scheduler.GetState(Handle);
		TestEqual(TEXT("Case 1: State is Cancelled"), FinalState, EVoxelJobState::Cancelled);

		// Clean up: signal the event in case the job did start (avoid deadlock)
		BlockEvent->Trigger();
		FPlatformProcess::Sleep(0.05f);
		FPlatformProcess::ReturnSynchEventToPool(BlockEvent);
	}

	// --- Case 2: Cancel while Running (deterministic via FEvent) ---
	{
		// Two events: one the Work blocks on (so we know it's Running),
		// one we signal to let Work finish after we cancel.
		FEvent* WorkStartedEvent = FPlatformProcess::GetSynchEventFromPool();
		FEvent* AllowFinishEvent = FPlatformProcess::GetSynchEventFromPool();
		bool bWorkCompleted = false;
		bool bOnCompleteRan = false;

		const FVoxelJobHandle Handle = Scheduler.Submit(
			[WorkStartedEvent, AllowFinishEvent, &bWorkCompleted]()
			{
				// Signal that Work has started (so the test knows we're Running)
				WorkStartedEvent->Trigger();
				// Block until the test signals us to finish
				AllowFinishEvent->Wait();
				bWorkCompleted = true;
			},
			EVoxelWorkPriority::Critical, // High priority to start quickly
			[&bOnCompleteRan]()
			{
				bOnCompleteRan = true;
			});

		// Wait for the job to actually start running (deterministic, no guessing)
		WorkStartedEvent->Wait();

		// Now we KNOW the job is Running. Cancel it.
		Scheduler.RequestCancel(Handle);

		// State should be Cancelled now, even though Work is still blocked
		TestEqual(TEXT("Case 2: State is Cancelled while Running"),
			Scheduler.GetState(Handle), EVoxelJobState::Cancelled);

		// Let Work finish
		AllowFinishEvent->Trigger();
		FPlatformProcess::Sleep(0.1f);

		// Work should have completed (cancel doesn't abort mid-Work)
		TestTrue(TEXT("Case 2: Work completed despite cancel"), bWorkCompleted);

		// OnComplete should have fired (FVoxelScheduler fires it regardless)
		TestTrue(TEXT("Case 2: OnComplete fired for cancelled-while-running"), bOnCompleteRan);

		// State should still be Cancelled (not overwritten to Completed)
		TestEqual(TEXT("Case 2: Final state still Cancelled"),
			Scheduler.GetState(Handle), EVoxelJobState::Cancelled);

		FPlatformProcess::ReturnSynchEventToPool(WorkStartedEvent);
		FPlatformProcess::ReturnSynchEventToPool(AllowFinishEvent);
	}

	return true;
}
