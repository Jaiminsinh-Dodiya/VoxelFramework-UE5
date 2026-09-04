// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelGenerationConfig.h"
#include "VoxelCoreTypes.h"
#include "VoxelChunk.h"

#if WITH_DEV_AUTOMATION_TESTS

// 1. Voxel.Generation.ConfigDeterminism
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationConfigDeterminismTest, "Voxel.Generation.ConfigDeterminism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelGenerationConfigDeterminismTest::RunTest(const FString& Parameters)
{
	const int32 TestChunkSize = 16;
	const FVoxelChunkCoordinate Coord(2, 3, 1);
	const int32 Seed = 9999;

	FVoxelGenerationConfig ConfigA;
	ConfigA.Terrain.BaseHeight = 70;
	ConfigA.Terrain.HeightAmplitude = 50.0f;

	FVoxelGenerationConfig ConfigB;
	ConfigB.Terrain.BaseHeight = 70;
	ConfigB.Terrain.HeightAmplitude = 50.0f;

	FVoxelGenerationPipeline PipelineA;
	FVoxelGenerationPipeline PipelineB;

	FVoxelChunk ChunkA(TestChunkSize);
	FVoxelChunk ChunkB(TestChunkSize);

	PipelineA.GenerateChunk(Seed, Coord, TestChunkSize, nullptr, {}, ChunkA, &ConfigA);
	PipelineB.GenerateChunk(Seed, Coord, TestChunkSize, nullptr, {}, ChunkB, &ConfigB);

	bool bAreIdentical = true;
	for (int32 X = 0; X < TestChunkSize; ++X)
	{
		for (int32 Y = 0; Y < TestChunkSize; ++Y)
		{
			for (int32 Z = 0; Z < TestChunkSize; ++Z)
			{
				if (ChunkA.GetBlock(X, Y, Z) != ChunkB.GetBlock(X, Y, Z))
				{
					bAreIdentical = false;
					break;
				}
			}
			if (!bAreIdentical) break;
		}
		if (!bAreIdentical) break;
	}

	TestTrue(TEXT("Identical configs with same seed should generate bit-exact identical chunks"), bAreIdentical);

	return true;
}

// 2. Voxel.Generation.ConfigCavesToggleable
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationConfigCavesToggleableTest, "Voxel.Generation.ConfigCavesToggleable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelGenerationConfigCavesToggleableTest::RunTest(const FString& Parameters)
{
	const int32 TestChunkSize = 16;
	const FVoxelChunkCoordinate Coord(0, 0, 1);
	const int32 Seed = 1234;

	FVoxelGenerationConfig ConfigWithCaves;
	ConfigWithCaves.Caves.bEnabled = true;

	FVoxelGenerationConfig ConfigWithoutCaves;
	ConfigWithoutCaves.Caves.bEnabled = false;

	FVoxelGenerationPipeline Pipeline;

	FVoxelChunk ChunkWithCaves(TestChunkSize);
	FVoxelChunk ChunkWithoutCaves(TestChunkSize);

	Pipeline.GenerateChunk(Seed, Coord, TestChunkSize, nullptr, {}, ChunkWithCaves, &ConfigWithCaves);
	Pipeline.GenerateChunk(Seed, Coord, TestChunkSize, nullptr, {}, ChunkWithoutCaves, &ConfigWithoutCaves);

	int32 SolidCountWithCaves = 0;
	int32 SolidCountWithoutCaves = 0;

	for (int32 X = 0; X < TestChunkSize; ++X)
	{
		for (int32 Y = 0; Y < TestChunkSize; ++Y)
		{
			for (int32 Z = 0; Z < TestChunkSize; ++Z)
			{
				if (ChunkWithCaves.GetBlock(X, Y, Z) != VoxelBlockId_Air)
				{
					SolidCountWithCaves++;
				}
				if (ChunkWithoutCaves.GetBlock(X, Y, Z) != VoxelBlockId_Air)
				{
					SolidCountWithoutCaves++;
				}
			}
		}
	}

	TestTrue(TEXT("Chunk without caves should have equal or more solid voxels than chunk with caves"), SolidCountWithoutCaves >= SolidCountWithCaves);

	return true;
}

// 3. Voxel.Generation.ConfigBaseHeightShift
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationConfigBaseHeightShiftTest, "Voxel.Generation.ConfigBaseHeightShift", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelGenerationConfigBaseHeightShiftTest::RunTest(const FString& Parameters)
{
	const int32 TestChunkSize = 16;
	const FVoxelChunkCoordinate Coord(0, 0, 2);
	const int32 Seed = 5555;

	FVoxelGenerationConfig ConfigLow;
	ConfigLow.Terrain.BaseHeight = 10;

	FVoxelGenerationConfig ConfigHigh;
	ConfigHigh.Terrain.BaseHeight = 80;


	FVoxelGenerationPipeline Pipeline;

	FVoxelChunk ChunkLow(TestChunkSize);
	FVoxelChunk ChunkHigh(TestChunkSize);

	Pipeline.GenerateChunk(Seed, Coord, TestChunkSize, nullptr, {}, ChunkLow, &ConfigLow);
	Pipeline.GenerateChunk(Seed, Coord, TestChunkSize, nullptr, {}, ChunkHigh, &ConfigHigh);

	int32 SolidCountLow = 0;
	int32 SolidCountHigh = 0;

	for (int32 X = 0; X < TestChunkSize; ++X)
	{
		for (int32 Y = 0; Y < TestChunkSize; ++Y)
		{
			for (int32 Z = 0; Z < TestChunkSize; ++Z)
			{
				if (ChunkLow.GetBlock(X, Y, Z) != VoxelBlockId_Air)
				{
					SolidCountLow++;
				}
				if (ChunkHigh.GetBlock(X, Y, Z) != VoxelBlockId_Air)
				{
					SolidCountHigh++;
				}
			}
		}
	}

	TestTrue(TEXT("Chunk with higher base height should have more solid voxels"), SolidCountHigh > SolidCountLow);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

