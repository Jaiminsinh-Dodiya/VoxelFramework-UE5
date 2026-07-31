// VoxelChunkStoreTests.cpp
//
// Verifies Phase 1's actual exit criterion: "can I create, store, modify,
// and query voxel data" - independent of generation, meshing, or rendering,
// none of which exist yet.

#include "Misc/AutomationTest.h"
#include "VoxelChunkStore.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelChunkStoreBasicTest, "Voxel.Storage.CreateStoreModifyQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelChunkStoreBasicTest::RunTest(const FString& Parameters)
{
	FVoxelChunkStore Store(/*ChunkSize=*/16);

	const FVoxelChunkCoordinate Coord(0, 0, 0);
	const FVoxelChunkHandle Handle = Store.CreateOrGetChunk(Coord);
	TestTrue(TEXT("Handle should be valid after creation"), Handle.IsValid());

	FVoxelChunk* Chunk = Store.FindChunkByHandle(Handle);
	TestNotNull(TEXT("Chunk should be resolvable from a fresh handle"), Chunk);
	if (!Chunk)
	{
		return false;
	}

	TestTrue(TEXT("New chunk should be empty (all air)"), Chunk->IsEmpty());

	// Generation write: should NOT be tracked as a diff.
	Chunk->SetBlock(1, 2, 3, /*BlockId=*/5, /*bIsGenerationWrite=*/true);
	TestEqual(TEXT("Block should read back what generation wrote"), Chunk->GetBlock(1, 2, 3), (FVoxelBlockId)5);
	TestEqual(TEXT("Generation writes must not appear in the modification diff"), Chunk->GetModifications().Num(), 0);
	TestFalse(TEXT("Chunk should no longer report empty after a non-air write"), Chunk->IsEmpty());

	// Gameplay edit: SHOULD be tracked as a diff.
	Chunk->SetBlock(1, 2, 3, /*BlockId=*/7, /*bIsGenerationWrite=*/false);
	TestEqual(TEXT("Block should read back the gameplay edit"), Chunk->GetBlock(1, 2, 3), (FVoxelBlockId)7);
	TestEqual(TEXT("Gameplay edit should appear exactly once in the modification diff"), Chunk->GetModifications().Num(), 1);

	// Stale-handle safety after removal.
	Store.RemoveChunk(Coord);
	TestNull(TEXT("Chunk should no longer resolve after removal"), Store.FindChunkByHandle(Handle));

	// Pool reuse should produce a NEW generation, invalidating the old handle even at the same coordinate.
	const FVoxelChunkHandle NewHandle = Store.CreateOrGetChunk(Coord);
	TestTrue(TEXT("Reused-slot chunk should still be valid via its new handle"), Store.FindChunkByHandle(NewHandle) != nullptr);
	TestNotEqual(TEXT("Reusing a pool slot must bump the generation counter"), NewHandle.Generation, Handle.Generation);
	TestNull(TEXT("Old handle must remain stale after slot reuse"), Store.FindChunkByHandle(Handle));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
