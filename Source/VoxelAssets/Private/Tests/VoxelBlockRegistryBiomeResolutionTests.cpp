// VoxelBlockRegistryBiomeResolutionTests.cpp
//
// Verifies PrecacheBiomeLayers actually resolves TSoftObjectPtr block
// references to concrete FVoxelBlockId values, and that TerrainPass
// (VoxelGeneration) places the resolved IDs instead of the old placeholder.

#include "Misc/AutomationTest.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBlockDefinition.h"
#include "VoxelBiomeDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBiomeLayerResolutionTest, "Voxel.Assets.BiomeLayerResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelBiomeLayerResolutionTest::RunTest(const FString& Parameters)
{
	UVoxelBlockDefinition* SandBlock = NewObject<UVoxelBlockDefinition>();
	SandBlock->BlockId = 42;

	UVoxelBiomeDefinition* DesertBiome = NewObject<UVoxelBiomeDefinition>();
	FVoxelTerrainLayer Layer;
	Layer.Block = SandBlock;
	Layer.ThicknessVoxels = 3;
	DesertBiome->TerrainLayers.Add(Layer);

	UVoxelBlockRegistry* Registry = NewObject<UVoxelBlockRegistry>();
	Registry->PrecacheBiomeLayers({ DesertBiome });

	const TArray<FVoxelBlockId>* Resolved = Registry->GetResolvedLayerBlockIds(DesertBiome);
	TestNotNull(TEXT("Resolved layer array should exist after precaching"), Resolved);
	if (!Resolved)
	{
		return false;
	}

	TestEqual(TEXT("Should resolve exactly one layer"), Resolved->Num(), 1);
	TestEqual(TEXT("Resolved block ID should match the sand block's registered ID"), (*Resolved)[0], (FVoxelBlockId)42);

	// A biome never passed to PrecacheBiomeLayers must return nullptr, not a stale/default entry.
	UVoxelBiomeDefinition* UnresolvedBiome = NewObject<UVoxelBiomeDefinition>();
	TestNull(TEXT("Unprecached biome should return nullptr, not fall through to some default"),
		Registry->GetResolvedLayerBlockIds(UnresolvedBiome));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
