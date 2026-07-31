// VoxelTerrainBiomeIntegrationTests.cpp
//
// Proves the fix end-to-end: a biome with a precached, resolved terrain
// layer actually causes TerrainPass to place that block, not the old
// hardcoded placeholder.

#include "Misc/AutomationTest.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelChunk.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBlockDefinition.h"
#include "VoxelBiomeDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTerrainBiomeIntegrationTest, "Voxel.Generation.TerrainRespectsBiomeLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelTerrainBiomeIntegrationTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 16;

	// A biome whose entire terrain is a single, distinctive block ID (99),
	// covering the whole climate range so BiomePass always selects it.
	UVoxelBlockDefinition* MarkerBlock = NewObject<UVoxelBlockDefinition>();
	MarkerBlock->BlockId = 99;

	UVoxelBiomeDefinition* UniformBiome = NewObject<UVoxelBiomeDefinition>();
	UniformBiome->TemperatureRange = FVector2D(0.0f, 1.0f);
	UniformBiome->HumidityRange = FVector2D(0.0f, 1.0f);
	FVoxelTerrainLayer Layer;
	Layer.Block = MarkerBlock;
	Layer.ThicknessVoxels = 999; // effectively the only/last layer, fills to bedrock
	UniformBiome->TerrainLayers.Add(Layer);

	UVoxelBlockRegistry* Registry = NewObject<UVoxelBlockRegistry>();
	Registry->PrecacheBiomeLayers({ UniformBiome });

	FVoxelGenerationPipeline Pipeline;
	FVoxelChunk Chunk(ChunkSize);

	Pipeline.GenerateChunk(
		/*WorldSeed=*/777,
		FVoxelChunkCoordinate(0, 0, 0),
		ChunkSize,
		Registry,
		TArray<const UVoxelBiomeDefinition*>{ UniformBiome },
		Chunk);

	// Find at least one solid (non-air) voxel and confirm it's the marker
	// block, not FallbackStoneId(1)/FallbackDirtId(2)/FallbackGrassId(3).
	bool bFoundSolidVoxel = false;
	bool bAllSolidAreMarker = true;
	for (int32 Z = 0; Z < ChunkSize; ++Z)
	{
		for (int32 Y = 0; Y < ChunkSize; ++Y)
		{
			for (int32 X = 0; X < ChunkSize; ++X)
			{
				const FVoxelBlockId Block = Chunk.GetBlock(X, Y, Z);
				if (Block != VoxelBlockId_Air)
				{
					bFoundSolidVoxel = true;
					if (Block != 99)
					{
						bAllSolidAreMarker = false;
					}
				}
			}
		}
	}

	TestTrue(TEXT("Chunk should contain solid terrain"), bFoundSolidVoxel);
	TestTrue(TEXT("Every solid voxel should be the biome's resolved marker block, not a placeholder ID"), bAllSolidAreMarker);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
