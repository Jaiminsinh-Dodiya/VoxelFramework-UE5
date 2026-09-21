// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "VoxelWorldSubsystem.h"
#include "VoxelWorldSettings.h"
#include "VoxelRuntimeSettings.h"
#include "VoxelWorldDefinition.h"
#include "VoxelGenerationDefinition.h"
#include "VoxelStreamingPreset.h"
#include "VoxelPhysicsPreset.h"
#include "VoxelBlockDefinition.h"
#include "VoxelBiomeDefinition.h"
#include "VoxelBlockRegistry.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelChunk.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// 1. Voxel.Integration.WorldDefinitionApplication
// Verifies that ApplyWorldDefinition correctly sets the subsystem's WorldSeed,
// VoxelWorldSize, generation config, biomes, and physics preset at runtime,
// and produces deterministic authored chunk generation.
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
	UVoxelBlockRegistry* Registry = World->GetSubsystem<UVoxelBlockRegistry>();
	TestNotNull(TEXT("UVoxelWorldSubsystem should be available"), Subsystem);
	TestNotNull(TEXT("UVoxelBlockRegistry should be available"), Registry);

	if (Subsystem && Registry)
	{
		// 1. Create Block Definitions
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

		// 2. Create Biome Definition with 3 layers
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

		// 3. Create Generation Definition
		UVoxelGenerationDefinition* GenDef = NewObject<UVoxelGenerationDefinition>(GetTransientPackage());
		GenDef->Terrain.BaseHeight = 80;
		GenDef->Terrain.HeightAmplitude = 20.0f;
		GenDef->Terrain.FallbackStoneBlock = StoneBlock;
		GenDef->Terrain.FallbackDirtBlock = DirtBlock;
		GenDef->Terrain.FallbackGrassBlock = GrassBlock;
		GenDef->Caves.bEnabled = true;

		// 4. Create Physics Preset
		UVoxelPhysicsPreset* PhysPreset = NewObject<UVoxelPhysicsPreset>(GetTransientPackage());
		PhysPreset->CollisionMode = EVoxelCollisionMode::Complex;
		PhysPreset->bAsyncCooking = false;
		PhysPreset->CollisionProfileName = TEXT("CustomVoxelProfile");

		// 5. Create Root World Definition
		UVoxelWorldDefinition* CustomWorldDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
		CustomWorldDef->WorldSeed = 887766;
		CustomWorldDef->VoxelWorldSize = 50.0f;
		CustomWorldDef->GenerationDefinition = GenDef;
		CustomWorldDef->Biomes.Add(PlainsBiome);
		CustomWorldDef->PhysicsPreset = PhysPreset;

		// 6. Apply World Definition
		Subsystem->ApplyWorldDefinition(CustomWorldDef);

		// 7. Verify Subsystem State
		TestEqual(TEXT("WorldSeed updated from WorldDefinition"), Subsystem->GetWorldSeed(), 887766);
		TestEqual(TEXT("VoxelWorldSize updated from WorldDefinition"), Subsystem->GetVoxelWorldSize(), 50.0f);
		TestEqual(TEXT("ActiveCollisionProfileName updated from PhysicsPreset"), Subsystem->GetActiveCollisionProfileName(), FName(TEXT("CustomVoxelProfile")));
		TestFalse(TEXT("ActiveAsyncCooking updated from PhysicsPreset"), Subsystem->GetActiveAsyncCooking());
		TestEqual(TEXT("ActiveCollisionMode updated from PhysicsPreset"), Subsystem->GetActiveCollisionMode(), EVoxelCollisionMode::Complex);
		TestEqual(TEXT("GenerationConfig BaseHeight matches definition"), Subsystem->GetGenerationConfig().Terrain.BaseHeight, 80);

		// 8. Test real chunk generation determinism with applied configuration
		FVoxelChunk ChunkA(Subsystem->GetChunkSize());
		FVoxelChunk ChunkB(Subsystem->GetChunkSize());
		const FVoxelChunkCoordinate TestCoord(0, 0, 2);

		FVoxelGenerationPipeline Pipeline;
		Pipeline.GenerateChunk(ChunkA, TestCoord, Subsystem->GetWorldSeed(), Registry, &Subsystem->GetGenerationConfig());
		Pipeline.GenerateChunk(ChunkB, TestCoord, Subsystem->GetWorldSeed(), Registry, &Subsystem->GetGenerationConfig());

		TestFalse(TEXT("Generated chunk should contain solid terrain blocks"), ChunkA.IsEmpty());
		TestTrue(TEXT("Chunk generation must be deterministic from applied definition"), ChunkA.GetDataCRC() == ChunkB.GetDataCRC());
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

// 2. Voxel.Integration.ConfigurationPrecedenceCascade
// Tests the full 4-tier configuration cascade:
// Tier 1: Project Settings Fallback
// Tier 2: World Definition (Seed, Scale, Biomes)
// Tier 3: Domain Presets (Physics & Streaming)
// Tier 4: Runtime Blueprint Overrides
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
		// -------------------------------------------------------------
		// Tier 1: Project Settings Default Baseline
		// -------------------------------------------------------------
		TestTrue(TEXT("Tier 1: Subsystem should be initialized"), Subsystem->IsWorldInitialized());
		TestEqual(TEXT("Tier 1: Baseline CollisionProfileName is BlockAll"), Subsystem->GetActiveCollisionProfileName(), FName(TEXT("BlockAll")));
		TestTrue(TEXT("Tier 1: Baseline AsyncCooking is enabled"), Subsystem->GetActiveAsyncCooking());

		// -------------------------------------------------------------
		// Tier 2: World Definition Override (Seed, Scale)
		// -------------------------------------------------------------
		UVoxelWorldDefinition* WorldDef = NewObject<UVoxelWorldDefinition>(GetTransientPackage());
		WorldDef->WorldSeed = 424242;
		WorldDef->VoxelWorldSize = 150.0f;

		// -------------------------------------------------------------
		// Tier 3: Domain Presets (Physics Preset)
		// -------------------------------------------------------------
		UVoxelPhysicsPreset* PhysPreset = NewObject<UVoxelPhysicsPreset>(GetTransientPackage());
		PhysPreset->CollisionMode = EVoxelCollisionMode::Complex;
		PhysPreset->bAsyncCooking = false;
		PhysPreset->CollisionProfileName = TEXT("PresetPhysicsProfile");
		WorldDef->PhysicsPreset = PhysPreset;

		Subsystem->ApplyWorldDefinition(WorldDef);

		TestEqual(TEXT("Tier 2: WorldSeed overridden by WorldDefinition"), Subsystem->GetWorldSeed(), 424242);
		TestEqual(TEXT("Tier 2: VoxelWorldSize overridden by WorldDefinition"), Subsystem->GetVoxelWorldSize(), 150.0f);
		TestEqual(TEXT("Tier 3: CollisionProfileName overridden by PhysicsPreset"), Subsystem->GetActiveCollisionProfileName(), FName(TEXT("PresetPhysicsProfile")));
		TestFalse(TEXT("Tier 3: AsyncCooking overridden by PhysicsPreset"), Subsystem->GetActiveAsyncCooking());

		// -------------------------------------------------------------
		// Tier 4: Runtime Dynamic Overrides
		// -------------------------------------------------------------
		Subsystem->SetActivePhysicsConfig(EVoxelCollisionMode::Complex, true, TEXT("RuntimeOverrideProfile"));

		TestEqual(TEXT("Tier 4: Runtime override takes precedence over preset"), Subsystem->GetActiveCollisionProfileName(), FName(TEXT("RuntimeOverrideProfile")));
		TestTrue(TEXT("Tier 4: Runtime async cooking override takes precedence"), Subsystem->GetActiveAsyncCooking());
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
