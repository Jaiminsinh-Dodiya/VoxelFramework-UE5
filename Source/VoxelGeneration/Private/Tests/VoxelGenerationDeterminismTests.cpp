// VoxelGenerationDeterminismTests.cpp
//
// Verifies the property ADR-005 (diff-based serialization) depends on:
// same seed + coordinate must always produce identical chunk contents.

#include "Misc/AutomationTest.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelChunk.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationDeterminismTest, "Voxel.Generation.DeterministicFromSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelGenerationDeterminismTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 16;
	const int32 Seed = 12345;
	const FVoxelChunkCoordinate Coord(2, -1, 3);

	FVoxelGenerationPipeline Pipeline;

	FVoxelChunk ChunkA(ChunkSize);
	FVoxelChunk ChunkB(ChunkSize);

	Pipeline.GenerateChunk(Seed, Coord, ChunkSize, /*BlockRegistry=*/nullptr, /*AvailableBiomes=*/{}, ChunkA);
	Pipeline.GenerateChunk(Seed, Coord, ChunkSize, /*BlockRegistry=*/nullptr, /*AvailableBiomes=*/{}, ChunkB);

	bool bAllMatch = true;
	for (int32 Z = 0; Z < ChunkSize && bAllMatch; ++Z)
	{
		for (int32 Y = 0; Y < ChunkSize && bAllMatch; ++Y)
		{
			for (int32 X = 0; X < ChunkSize && bAllMatch; ++X)
			{
				if (ChunkA.GetBlock(X, Y, Z) != ChunkB.GetBlock(X, Y, Z))
				{
					bAllMatch = false;
				}
			}
		}
	}

	TestTrue(TEXT("Same seed+coordinate must produce byte-identical chunk contents"), bAllMatch);
	TestFalse(TEXT("Generated terrain chunk should not be all-air"), ChunkA.IsEmpty());

	// Different coordinate, same seed, should (almost certainly) differ - a
	// weak but useful sanity check that the coordinate is actually being
	// used to vary output, not just the seed.
	FVoxelChunk ChunkC(ChunkSize);
	Pipeline.GenerateChunk(Seed, FVoxelChunkCoordinate(50, 50, 3), ChunkSize, nullptr, {}, ChunkC);

	bool bAnyDifferent = false;
	for (int32 Z = 0; Z < ChunkSize && !bAnyDifferent; ++Z)
		for (int32 Y = 0; Y < ChunkSize && !bAnyDifferent; ++Y)
			for (int32 X = 0; X < ChunkSize && !bAnyDifferent; ++X)
				if (ChunkA.GetBlock(X, Y, Z) != ChunkC.GetBlock(X, Y, Z))
					bAnyDifferent = true;

	TestTrue(TEXT("Different coordinates should produce different terrain"), bAnyDifferent);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
