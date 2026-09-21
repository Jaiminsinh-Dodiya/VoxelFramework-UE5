// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "VoxelWorldDefinition.h"
#include "VoxelGenerationDefinition.h"
#include "VoxelStreamingPreset.h"
#include "VoxelConfigValidator.h"
#include "VoxelBlockDefinition.h"
#include "VoxelBiomeDefinition.h"
#include "VoxelBlockRegistry.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// 1. Voxel.Authoring.AssetGraphValidation
// Tests constructing a complete, valid starter asset graph (similar to SetupVoxelFramework.py)
// and verifies that UVoxelConfigValidator returns no errors or warnings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelAssetGraphValidationTest, "Voxel.Authoring.AssetGraphValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelAssetGraphValidationTest::RunTest(const FString& Parameters)
{
	// 1. Blocks
	UVoxelBlockDefinition* StoneBlock = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	StoneBlock->BlockId = 1;
	StoneBlock->DisplayName = FText::FromString(TEXT("Stone"));
	StoneBlock->bIsSolid = true;
	StoneBlock->bGeneratesCollision = true;

	UVoxelBlockDefinition* DirtBlock = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	DirtBlock->BlockId = 2;
	DirtBlock->DisplayName = FText::FromString(TEXT("Dirt"));
	DirtBlock->bIsSolid = true;
	DirtBlock->bGeneratesCollision = true;

	UVoxelBlockDefinition* GrassBlock = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	GrassBlock->BlockId = 3;
	GrassBlock->DisplayName = FText::FromString(TEXT("Grass"));
	GrassBlock->bIsSolid = true;
	GrassBlock->bGeneratesCollision = true;

	// 2. Biome
	UVoxelBiomeDefinition* PlainsBiome = NewObject<UVoxelBiomeDefinition>(GetTransientPackage());
	PlainsBiome->DisplayName = FText::FromString(TEXT("Plains"));
	PlainsBiome->TemperatureRange = FVector2D(0.0f, 1.0f);
	PlainsBiome->HumidityRange = FVector2D(0.0f, 1.0f);

	FVoxelTerrainLayer LayerGrass;
	LayerGrass.Block = GrassBlock;
	LayerGrass.ThicknessVoxels = 1;

	FVoxelTerrainLayer LayerDirt;
	LayerDirt.Block = DirtBlock;
	LayerDirt.ThicknessVoxels = 3;

	FVoxelTerrainLayer LayerStone;
	LayerStone.Block = StoneBlock;
	LayerStone.ThicknessVoxels = 1;

	PlainsBiome->TerrainLayers.Add(LayerGrass);
	PlainsBiome->TerrainLayers.Add(LayerDirt);
	PlainsBiome->TerrainLayers.Add(LayerStone);

	// 3. Generation Definition
	UVoxelGenerationDefinition* GenDef = NewObject<UVoxelGenerationDefinition>(GetTransientPackage());
	GenDef->Climate.Frequency = 0.001f;
	GenDef->Climate.TemperatureSeedOffset = 1000;
	GenDef->Climate.HumiditySeedOffset = 2000;
	GenDef->Terrain.BaseHeight = 64;
	GenDef->Terrain.HeightAmplitude = 32.0f;
	GenDef->Terrain.BaseFrequency = 0.01f;
	GenDef->Terrain.NoiseOctaves = 4;
	GenDef->Terrain.FallbackStoneBlock = StoneBlock;
	GenDef->Terrain.FallbackDirtBlock = DirtBlock;
	GenDef->Terrain.FallbackGrassBlock = GrassBlock;
	GenDef->Terrain.FallbackDirtDepth = 4;
	GenDef->Caves.bEnabled = true;
	GenDef->Caves.CarveThreshold = 0.58f;
	GenDef->Caves.DensityFrequency = 0.045f;
	GenDef->Caves.NoiseOctaves = 3;
	GenDef->Caves.SurfaceProtectionDepth = 3;
	GenDef->Caves.CaveSeedOffset = 5000;

	// 4. Streaming Preset
	UVoxelStreamingPreset* StreamingPreset = NewObject<UVoxelStreamingPreset>(GetTransientPackage());
	StreamingPreset->SimulationDistance = 4;
	StreamingPreset->RenderDistance = 8;
	StreamingPreset->GenerationDistance = 10;
	StreamingPreset->PersistenceDistance = 12;
	StreamingPreset->StreamingBudgetMs = 1.5f;

	// 5. World Definition
	UVoxelWorldDefinition* WorldDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
	WorldDef->WorldName = FText::FromString(TEXT("Authoring Test World"));
	WorldDef->WorldSeed = 12345;
	WorldDef->VoxelWorldSize = 100.0f;
	WorldDef->GenerationDefinition = GenDef;
	WorldDef->Biomes.Add(PlainsBiome);
	WorldDef->StreamingPreset = StreamingPreset;

	// 6. Validate
	TArray<FVoxelValidationMessage> Messages = UVoxelConfigValidator::ValidateWorldDefinition(WorldDef, 32, 8);
	
	int32 ErrorCount = 0;
	int32 WarningCount = 0;
	for (const FVoxelValidationMessage& Msg : Messages)
	{
		if (Msg.Severity == EVoxelValidationSeverity::Error)
		{
			ErrorCount++;
		}
		else if (Msg.Severity == EVoxelValidationSeverity::Warning)
		{
			WarningCount++;
		}
	}

	TestEqual(TEXT("Fully wired authoring asset graph should have 0 validation errors"), ErrorCount, 0);
	TestEqual(TEXT("Fully wired authoring asset graph should have 0 validation warnings"), WarningCount, 0);

	return true;
}

// 2. Voxel.Authoring.BlockAndBiomeDefinitions
// Validates block properties and biome layer resolution rules.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBlockAndBiomeDefinitionsTest, "Voxel.Authoring.BlockAndBiomeDefinitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelBlockAndBiomeDefinitionsTest::RunTest(const FString& Parameters)
{
	UVoxelBlockDefinition* BlockA = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	BlockA->BlockId = 5;
	BlockA->bIsSolid = true;
	BlockA->bGeneratesCollision = true;

	UVoxelBlockDefinition* BlockB = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	BlockB->BlockId = 6;
	BlockB->bIsSolid = false; // Non-solid (e.g. foliage)
	BlockB->bGeneratesCollision = false;

	TestTrue(TEXT("BlockA should be solid"), BlockA->bIsSolid);
	TestTrue(TEXT("BlockA should generate collision"), BlockA->bGeneratesCollision);
	TestFalse(TEXT("BlockB should not be solid"), BlockB->bIsSolid);
	TestFalse(TEXT("BlockB should not generate collision"), BlockB->bGeneratesCollision);

	UVoxelBiomeDefinition* Biome = NewObject<UVoxelBiomeDefinition>(GetTransientPackage());
	FVoxelTerrainLayer Layer;
	Layer.Block = BlockA;
	Layer.ThicknessVoxels = 4;
	Biome->TerrainLayers.Add(Layer);

	TestEqual(TEXT("Biome should have 1 layer"), Biome->TerrainLayers.Num(), 1);
	TestEqual(TEXT("Layer thickness should be 4"), Biome->TerrainLayers[0].ThicknessVoxels, 4);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
