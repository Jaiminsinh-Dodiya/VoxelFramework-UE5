// VoxelWorldSubsystemTests.cpp
//
// Deliberately scoped to behavior observable WITHOUT waiting for the
// dispatched worker-thread job to complete: RequestChunk's idempotency,
// IsChunkReady correctly reporting false before the async job has had a
// chance to run/be pumped, and UnloadChunk's cleanup. This does NOT
// validate the actual generation->meshing->rendering pipeline end-to-end -
// doing that safely inside a synchronous automation test would require
// manually pumping AsyncTask's Game Thread queue and the task graph, which
// is fragile enough to test infrastructure that a false pass/fail here
// would be more misleading than reassuring. The real validation for the
// full pipeline is a live PIE/level test - same honesty pattern as
// VoxelRendering's scene proxy, which also isn't fully unit-testable.

#include "Misc/AutomationTest.h"
#include "VoxelWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelWorldSubsystemBasicTest, "Voxel.World.RequestUnloadBookkeeping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelWorldSubsystemBasicTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Test world should be created"), World);
	if (!World)
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());

	UVoxelWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelWorldSubsystem>();
	TestNotNull(TEXT("UVoxelWorldSubsystem should be available on any World"), Subsystem);

	if (Subsystem)
	{
		const FVoxelChunkCoordinate Coord(0, 0, 0);

		const FVoxelChunkHandle HandleA = Subsystem->RequestChunk(Coord);
		TestTrue(TEXT("RequestChunk should return a valid handle"), HandleA.IsValid());

		// The worker-thread job hasn't been pumped/completed yet (this test
		// never yields), so the chunk must NOT be reported ready - this
		// assertion is safe and non-flaky precisely because we haven't
		// given the async machinery any chance to run.
		TestFalse(TEXT("Chunk should not be ready immediately after requesting (async job hasn't completed)"), Subsystem->IsChunkReady(Coord));

		const FVoxelChunkHandle HandleB = Subsystem->RequestChunk(Coord);
		TestEqual(TEXT("Requesting the same coordinate twice should be idempotent (same generation)"), HandleA.Generation, HandleB.Generation);

		Subsystem->UnloadChunk(Coord);
		TestNull(TEXT("Chunk should no longer be found after UnloadChunk"), Subsystem->FindChunk(Coord));
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
