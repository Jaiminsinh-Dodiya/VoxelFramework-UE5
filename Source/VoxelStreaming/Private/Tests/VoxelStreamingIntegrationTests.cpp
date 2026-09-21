// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "VoxelWorldSubsystem.h"
#include "VoxelStreamingManager.h"
#include "VoxelWorldSettings.h"
#include "VoxelRuntimeSettings.h"
#include "VoxelWorldDefinition.h"
#include "VoxelGenerationDefinition.h"
#include "VoxelStreamingPreset.h"
#include "VoxelPhysicsPreset.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// Voxel.Integration.ConfigurationPrecedenceCascade
// Tests the full 4-tier configuration cascade across both WorldSubsystem and StreamingManager:
// Tier 1: Project Settings Baseline
// Tier 2: World Definition (Seed, Scale)
// Tier 3: Domain Presets (Physics & Streaming via OnWorldDefinitionApplied delegate)
// Tier 4: Runtime Dynamic Overrides
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

	UVoxelWorldSubsystem* WorldSubsystem = World->GetSubsystem<UVoxelWorldSubsystem>();
	UVoxelStreamingManager* StreamingManager = World->GetSubsystem<UVoxelStreamingManager>();
	TestNotNull(TEXT("UVoxelWorldSubsystem should be available"), WorldSubsystem);
	TestNotNull(TEXT("UVoxelStreamingManager should be available"), StreamingManager);

	if (WorldSubsystem && StreamingManager)
	{
		// -------------------------------------------------------------
		// Tier 1: Project Settings Default Baseline
		// -------------------------------------------------------------
		TestTrue(TEXT("Tier 1: WorldSubsystem is initialized"), WorldSubsystem->IsWorldInitialized());
		TestEqual(TEXT("Tier 1: Baseline CollisionProfileName is BlockAll"), WorldSubsystem->GetActiveCollisionProfileName(), FName(TEXT("BlockAll")));
		TestTrue(TEXT("Tier 1: Baseline AsyncCooking is enabled"), WorldSubsystem->GetActiveAsyncCooking());
		TestEqual(TEXT("Tier 1: Baseline SimulationDistance is 4"), StreamingManager->GetSimulationDistance(), 4);
		TestEqual(TEXT("Tier 1: Baseline RenderDistance is 8"), StreamingManager->GetRenderDistance(), 8);
		TestEqual(TEXT("Tier 1: Baseline GenerationDistance is 10"), StreamingManager->GetGenerationDistance(), 10);
		TestEqual(TEXT("Tier 1: Baseline PersistenceDistance is 12"), StreamingManager->GetPersistenceDistance(), 12);
		TestEqual(TEXT("Tier 1: Baseline StreamingBudgetMs is 1.5ms"), StreamingManager->GetStreamingBudgetMs(), 1.5f);

		// -------------------------------------------------------------
		// Tier 2 & Tier 3: World Definition with Physics & Streaming Presets
		// -------------------------------------------------------------
		UVoxelWorldDefinition* WorldDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
		WorldDef->WorldSeed = 424242;
		WorldDef->VoxelWorldSize = 150.0f;

		// Tier 3a: Physics Preset
		UVoxelPhysicsPreset* PhysPreset = NewObject<UVoxelPhysicsPreset>(GetTransientPackage());
		PhysPreset->CollisionMode = EVoxelCollisionMode::Complex;
		PhysPreset->bAsyncCooking = false;
		PhysPreset->CollisionProfileName = TEXT("PresetPhysicsProfile");
		WorldDef->PhysicsPreset = PhysPreset;

		// Tier 3b: Streaming Preset
		UVoxelStreamingPreset* StreamingPreset = NewObject<UVoxelStreamingPreset>(GetTransientPackage());
		StreamingPreset->SimulationDistance = 2;
		StreamingPreset->RenderDistance = 5;
		StreamingPreset->GenerationDistance = 7;
		StreamingPreset->PersistenceDistance = 9;
		StreamingPreset->StreamingBudgetMs = 3.0f;
		WorldDef->StreamingPreset = StreamingPreset;

		// Apply World Definition -> Broadcasts OnWorldDefinitionApplied
		WorldSubsystem->ApplyWorldDefinition(WorldDef);

		// Verify Tier 2 World Definition overrides
		TestEqual(TEXT("Tier 2: WorldSeed overridden by WorldDefinition"), WorldSubsystem->GetWorldSeed(), 424242);
		TestEqual(TEXT("Tier 2: VoxelWorldSize overridden by WorldDefinition"), WorldSubsystem->GetVoxelWorldSize(), 150.0f);

		// Verify Tier 3 Physics Preset overrides
		TestEqual(TEXT("Tier 3: CollisionProfileName overridden by PhysicsPreset"), WorldSubsystem->GetActiveCollisionProfileName(), FName(TEXT("PresetPhysicsProfile")));
		TestFalse(TEXT("Tier 3: AsyncCooking overridden by PhysicsPreset"), WorldSubsystem->GetActiveAsyncCooking());

		// Verify Tier 3 Streaming Preset overrides (propagated via delegate!)
		TestEqual(TEXT("Tier 3: SimulationDistance propagated from StreamingPreset"), StreamingManager->GetSimulationDistance(), 2);
		TestEqual(TEXT("Tier 3: RenderDistance propagated from StreamingPreset"), StreamingManager->GetRenderDistance(), 5);
		TestEqual(TEXT("Tier 3: GenerationDistance propagated from StreamingPreset"), StreamingManager->GetGenerationDistance(), 7);
		TestEqual(TEXT("Tier 3: PersistenceDistance propagated from StreamingPreset"), StreamingManager->GetPersistenceDistance(), 9);
		TestEqual(TEXT("Tier 3: StreamingBudgetMs propagated from StreamingPreset"), StreamingManager->GetStreamingBudgetMs(), 3.0f);

		// -------------------------------------------------------------
		// Tier 4: Runtime Dynamic Overrides (take final precedence)
		// -------------------------------------------------------------
		WorldSubsystem->SetActivePhysicsConfig(EVoxelCollisionMode::Complex, true, TEXT("RuntimeOverrideProfile"));
		StreamingManager->SetRenderDistance(12);
		StreamingManager->SetStreamingBudgetMs(5.0f);

		TestEqual(TEXT("Tier 4: Runtime physics profile override takes precedence"), WorldSubsystem->GetActiveCollisionProfileName(), FName(TEXT("RuntimeOverrideProfile")));
		TestTrue(TEXT("Tier 4: Runtime async cooking override takes precedence"), WorldSubsystem->GetActiveAsyncCooking());
		TestEqual(TEXT("Tier 4: Runtime render distance override takes precedence"), StreamingManager->GetRenderDistance(), 12);
		TestEqual(TEXT("Tier 4: Runtime streaming budget override takes precedence"), StreamingManager->GetStreamingBudgetMs(), 5.0f);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
