// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "VoxelWorldDefinition.h"
#include "VoxelGenerationDefinition.h"
#include "VoxelStreamingPreset.h"
#include "VoxelConfigValidator.h"
#include "VoxelBlockDefinition.h"
#include "VoxelBiomeDefinition.h"
#include "VoxelBlockRegistry.h"
#include "VoxelGenerationConfig.h"
#include "Engine/World.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// 1. Voxel.Configuration.WorldDefinitionDefaults
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelWorldDefinitionDefaultsTest, "Voxel.Configuration.WorldDefinitionDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelWorldDefinitionDefaultsTest::RunTest(const FString& Parameters)
{
	UVoxelWorldDefinition* WorldDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
	TestEqual(TEXT("Default WorldSeed should be 1234"), WorldDef->WorldSeed, 1234);
	TestEqual(TEXT("Default VoxelWorldSize should be 100.0f"), WorldDef->VoxelWorldSize, 100.0f);
	TestTrue(TEXT("Default GenerationDefinition should be null"), WorldDef->GenerationDefinition.IsNull());
	TestTrue(TEXT("Default StreamingPreset should be null"), WorldDef->StreamingPreset.IsNull());
	TestTrue(TEXT("Default PhysicsPreset should be null"), WorldDef->PhysicsPreset.IsNull());

	return true;
}

// 2. Voxel.Configuration.GenerationConfigFromDefinition
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationConfigFromDefinitionTest, "Voxel.Configuration.GenerationConfigFromDefinition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelGenerationConfigFromDefinitionTest::RunTest(const FString& Parameters)
{
	UVoxelGenerationDefinition* GenDef = NewObject<UVoxelGenerationDefinition>(GetTransientPackage());
	GenDef->Terrain.BaseHeight = 80;
	GenDef->Caves.CarveThreshold = 0.65f;

	FVoxelGenerationConfig Config = GenDef->ToRuntimeConfig(nullptr);

	TestEqual(TEXT("BaseHeight should be 80"), Config.Terrain.BaseHeight, 80);
	TestEqual(TEXT("CarveThreshold should be 0.65f"), Config.Caves.CarveThreshold, 0.65f);

	return true;
}

// 3. Voxel.Configuration.StreamingPresetApply
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelStreamingPresetApplyTest, "Voxel.Configuration.StreamingPresetApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelStreamingPresetApplyTest::RunTest(const FString& Parameters)
{
	UVoxelStreamingPreset* Preset = NewObject<UVoxelStreamingPreset>(GetTransientPackage());
	Preset->SimulationDistance = 3;
	Preset->RenderDistance = 6;
	Preset->GenerationDistance = 8;
	Preset->PersistenceDistance = 10;
	Preset->StreamingBudgetMs = 2.0f;

	TestEqual(TEXT("SimulationDistance should be 3"), Preset->SimulationDistance, 3);
	TestEqual(TEXT("RenderDistance should be 6"), Preset->RenderDistance, 6);
	TestEqual(TEXT("GenerationDistance should be 8"), Preset->GenerationDistance, 8);
	TestEqual(TEXT("PersistenceDistance should be 10"), Preset->PersistenceDistance, 10);
	TestEqual(TEXT("StreamingBudgetMs should be 2.0f"), Preset->StreamingBudgetMs, 2.0f);

	return true;
}

// 4. Voxel.Configuration.ValidationErrors
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelValidationErrorsTest, "Voxel.Configuration.ValidationErrors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelValidationErrorsTest::RunTest(const FString& Parameters)
{
	UVoxelWorldDefinition* WorldDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
	// Intentionally omitting GenerationDefinition

	TArray<FVoxelValidationMessage> Messages = UVoxelConfigValidator::ValidateWorldDefinition(WorldDef);

	bool bFoundError = false;
	for (const FVoxelValidationMessage& Msg : Messages)
	{
		if (Msg.Severity == EVoxelValidationSeverity::Error && Msg.Message.Contains(TEXT("Missing Generation Definition")))
		{
			bFoundError = true;
			break;
		}
	}
	TestTrue(TEXT("Error message should contain 'Missing Generation Definition'"), bFoundError);

	return true;
}

// 5. Voxel.Configuration.ValidationWarnings
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelValidationWarningsTest, "Voxel.Configuration.ValidationWarnings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelValidationWarningsTest::RunTest(const FString& Parameters)
{
	UVoxelGenerationDefinition* GenDef = NewObject<UVoxelGenerationDefinition>(GetTransientPackage());
	GenDef->Terrain.BaseHeight = -50; // Invalid height

	TArray<FVoxelValidationMessage> Messages = UVoxelConfigValidator::ValidateGenerationDefinition(GenDef);

	bool bFoundWarning = false;
	for (const FVoxelValidationMessage& Msg : Messages)
	{
		if (Msg.Severity == EVoxelValidationSeverity::Warning && Msg.Message.Contains(TEXT("BaseHeight")))
		{
			bFoundWarning = true;
			break;
		}
	}
	TestTrue(TEXT("Warning message should mention BaseHeight"), bFoundWarning);

	return true;
}

// 6. Voxel.Configuration.BlockDefinitionValidation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBlockDefinitionValidationTest, "Voxel.Configuration.BlockDefinitionValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelBlockDefinitionValidationTest::RunTest(const FString& Parameters)
{
	UVoxelBlockDefinition* BlockA = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	BlockA->BlockId = 5;

	UVoxelBlockDefinition* BlockB = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	BlockB->BlockId = 5;

	TArray<UVoxelBlockDefinition*> Blocks = { BlockA, BlockB };
	TArray<FVoxelValidationMessage> Messages = UVoxelConfigValidator::ValidateBlockDefinitions(Blocks);

	bool bFoundDupError = false;
	for (const FVoxelValidationMessage& Msg : Messages)
	{
		if (Msg.Severity == EVoxelValidationSeverity::Error && Msg.Message.Contains(TEXT("Duplicate BlockId")))
		{
			bFoundDupError = true;
			break;
		}
	}
	TestTrue(TEXT("Should have errors for duplicate block IDs"), bFoundDupError);

	UVoxelBlockDefinition* BlockAir = NewObject<UVoxelBlockDefinition>(GetTransientPackage());
	BlockAir->BlockId = 0; // Reserved ID

	TArray<UVoxelBlockDefinition*> BlocksAir = { BlockAir };
	TArray<FVoxelValidationMessage> MessagesAir = UVoxelConfigValidator::ValidateBlockDefinitions(BlocksAir);

	bool bFoundAirError = false;
	for (const FVoxelValidationMessage& Msg : MessagesAir)
	{
		if (Msg.Severity == EVoxelValidationSeverity::Error && Msg.Message.Contains(TEXT("reserved")))
		{
			bFoundAirError = true;
			break;
		}
	}
	TestTrue(TEXT("Should have errors for reserved block ID (0)"), bFoundAirError);

	return true;
}

// 7. Voxel.Configuration.BiomeDefinitionValidation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBiomeDefinitionValidationTest, "Voxel.Configuration.BiomeDefinitionValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoxelBiomeDefinitionValidationTest::RunTest(const FString& Parameters)
{
	UVoxelBiomeDefinition* BiomeDef = NewObject<UVoxelBiomeDefinition>(GetTransientPackage());
	BiomeDef->TerrainLayers.Empty();

	TArray<FVoxelValidationMessage> Messages = UVoxelConfigValidator::ValidateBiomeDefinition(BiomeDef);

	bool bFoundWarning = false;
	for (const FVoxelValidationMessage& Msg : Messages)
	{
		if (Msg.Severity == EVoxelValidationSeverity::Warning && Msg.Message.Contains(TEXT("terrain layers")))
		{
			bFoundWarning = true;
			break;
		}
	}
	TestTrue(TEXT("Should have warnings for empty terrain layers"), bFoundWarning);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS


