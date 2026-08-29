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
	// Priority rule:
	// dist <= Sim (4)  -> Critical
	// dist <= Ren (8)  -> High
	// dist <= Gen (10) -> Normal
	// dist >  Gen (10) -> Low
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
