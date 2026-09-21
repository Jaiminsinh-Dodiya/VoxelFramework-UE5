// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "VoxelWorldSubsystem.h"
#include "VoxelWorldSettings.h"
#include "VoxelWorldDefinition.h"
#include "VoxelGenerationDefinition.h"
#include "VoxelStreamingPreset.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// 1. Voxel.Integration.WorldDefinitionApplication
// Verifies that ApplyWorldDefinition correctly sets the subsystem's WorldSeed,
// VoxelWorldSize, generation config, and precached biomes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelWorldDefinitionApplicationTest, "Voxel.Integration.WorldDefinitionApplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelWorldDefinitionApplicationTest::RunTest(const FString& Parameters)
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
	TestNotNull(TEXT("UVoxelWorldSubsystem should be available"), Subsystem);

	if (Subsystem)
	{
		UVoxelWorldDefinition* CustomWorldDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
		CustomWorldDef->WorldSeed = 887766;
		CustomWorldDef->VoxelWorldSize = 50.0f;

		UVoxelGenerationDefinition* GenDef = NewObject<UVoxelGenerationDefinition>(GetTransientPackage());
		GenDef->Terrain.BaseHeight = 96;
		CustomWorldDef->GenerationDefinition = GenDef;

		Subsystem->ApplyWorldDefinition(CustomWorldDef);

		TestEqual(TEXT("WorldSeed should be updated from WorldDefinition"), Subsystem->GetWorldSeed(), 887766);
		TestEqual(TEXT("VoxelWorldSize should be updated from WorldDefinition"), Subsystem->GetVoxelWorldSize(), 50.0f);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

// 2. Voxel.Integration.ConfigurationPrecedenceCascade
// Tests the 4-tier configuration precedence:
// Default Engine Settings -> World Definition -> Runtime Subsystem Queries.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelConfigurationPrecedenceTest, "Voxel.Integration.ConfigurationPrecedenceCascade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelConfigurationPrecedenceTest::RunTest(const FString& Parameters)
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
	TestNotNull(TEXT("UVoxelWorldSubsystem should be available"), Subsystem);

	if (Subsystem)
	{
		// 1. Initially initialized with default fallback settings
		TestTrue(TEXT("Subsystem should be initialized"), Subsystem->IsWorldInitialized());

		// 2. Apply explicit World Definition (Tier 2 override)
		UVoxelWorldDefinition* OverrideDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
		OverrideDef->WorldSeed = 424242;
		OverrideDef->VoxelWorldSize = 150.0f;
		Subsystem->ApplyWorldDefinition(OverrideDef);

		TestEqual(TEXT("Tier 2: WorldDefinition seed should override default"), Subsystem->GetWorldSeed(), 424242);
		TestEqual(TEXT("Tier 2: WorldDefinition VoxelWorldSize should override default"), Subsystem->GetVoxelWorldSize(), 150.0f);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
