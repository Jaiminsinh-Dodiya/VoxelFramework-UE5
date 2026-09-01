// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelConfigValidator.h"
#include "VoxelWorldDefinition.h"
#include "VoxelGenerationDefinition.h"
#include "VoxelBiomeDefinition.h"
#include "VoxelBlockDefinition.h"
#include "VoxelCoreTypes.h"

TArray<FVoxelValidationMessage> UVoxelConfigValidator::ValidateWorldDefinition(const UVoxelWorldDefinition* WorldDef, int32 ChunkSize, int32 WorldHeightInChunks)
{
	TArray<FVoxelValidationMessage> Messages;

	if (!WorldDef)
	{
		Messages.Emplace(EVoxelValidationSeverity::Error, TEXT("World Definition is null"));
		return Messages;
	}

	if (WorldDef->GenerationDefinition.IsNull())
	{
		Messages.Emplace(EVoxelValidationSeverity::Error, TEXT("Missing Generation Definition"), TEXT("Assign a UVoxelGenerationDefinition asset to GenerationDefinition"));
	}
	else
	{
		UVoxelGenerationDefinition* GenDef = WorldDef->GenerationDefinition.LoadSynchronous();
		if (GenDef)
		{
			Messages.Append(ValidateGenerationDefinition(GenDef, ChunkSize, WorldHeightInChunks));
		}
	}

	if (WorldDef->VoxelWorldSize <= 0.0f)
	{
		Messages.Emplace(EVoxelValidationSeverity::Error, TEXT("VoxelWorldSize must be greater than 0"));
	}

	if (WorldDef->Biomes.Num() == 0)
	{
		Messages.Emplace(EVoxelValidationSeverity::Warning, TEXT("No biomes assigned; terrain will use fallback blocks"), TEXT("Add biomes to Biomes array or ensure fallback blocks are set"));
	}
	else
	{
		for (const TSoftObjectPtr<UVoxelBiomeDefinition>& BiomePtr : WorldDef->Biomes)
		{
			if (!BiomePtr.IsNull())
			{
				UVoxelBiomeDefinition* BiomeDef = BiomePtr.LoadSynchronous();
				if (BiomeDef)
				{
					Messages.Append(ValidateBiomeDefinition(BiomeDef));
				}
			}
		}
	}

	if (WorldDef->StreamingPreset.IsNull())
	{
		Messages.Emplace(EVoxelValidationSeverity::Info, TEXT("No streaming preset assigned; project defaults will be used"));
	}

	return Messages;
}

TArray<FVoxelValidationMessage> UVoxelConfigValidator::ValidateGenerationDefinition(const UVoxelGenerationDefinition* GenDef, int32 ChunkSize, int32 WorldHeightInChunks)
{
	TArray<FVoxelValidationMessage> Messages;

	if (!GenDef)
	{
		Messages.Emplace(EVoxelValidationSeverity::Error, TEXT("Generation Definition is null"));
		return Messages;
	}

	if (GenDef->Terrain.NoiseOctaves < 1 || GenDef->Terrain.NoiseOctaves > 8)
	{
		Messages.Emplace(EVoxelValidationSeverity::Warning, TEXT("Terrain NoiseOctaves should be between 1 and 8"));
	}

	int32 MaxHeight = WorldHeightInChunks * ChunkSize;
	if (GenDef->Terrain.BaseHeight < 0 || GenDef->Terrain.BaseHeight > MaxHeight)
	{
		Messages.Emplace(EVoxelValidationSeverity::Warning, TEXT("BaseHeight is outside valid world height range"));
	}

	if (GenDef->Terrain.HeightAmplitude < 0.0f)
	{
		Messages.Emplace(EVoxelValidationSeverity::Warning, TEXT("HeightAmplitude should be non-negative"));
	}

	if (GenDef->Caves.bEnabled)
	{
		if (GenDef->Caves.CarveThreshold < 0.0f || GenDef->Caves.CarveThreshold > 1.0f)
		{
			Messages.Emplace(EVoxelValidationSeverity::Warning, TEXT("Cave CarveThreshold should be between 0.0 and 1.0"));
		}

		if (GenDef->Caves.SurfaceProtectionDepth < 0)
		{
			Messages.Emplace(EVoxelValidationSeverity::Warning, TEXT("SurfaceProtectionDepth should be >= 0"));
		}
	}

	return Messages;
}

TArray<FVoxelValidationMessage> UVoxelConfigValidator::ValidateBlockDefinitions(const TArray<UVoxelBlockDefinition*>& BlockDefs)
{
	TArray<FVoxelValidationMessage> Messages;
	TSet<int32> SeenIds;

	for (const UVoxelBlockDefinition* BlockDef : BlockDefs)
	{
		if (!BlockDef)
		{
			continue;
		}

		if (BlockDef->BlockId == VoxelBlockId_Air)
		{
			Messages.Emplace(EVoxelValidationSeverity::Error, TEXT("BlockId 0 is reserved for VoxelBlockId_Air"));
		}
		else if (SeenIds.Contains(BlockDef->BlockId))
		{
			Messages.Emplace(EVoxelValidationSeverity::Error, FString::Printf(TEXT("Duplicate BlockId found: %d"), BlockDef->BlockId));
		}
		else
		{
			SeenIds.Add(BlockDef->BlockId);
		}
	}

	return Messages;
}

TArray<FVoxelValidationMessage> UVoxelConfigValidator::ValidateBiomeDefinition(const UVoxelBiomeDefinition* BiomeDef)
{
	TArray<FVoxelValidationMessage> Messages;

	if (!BiomeDef)
	{
		Messages.Emplace(EVoxelValidationSeverity::Error, TEXT("Biome Definition is null"));
		return Messages;
	}

	if (BiomeDef->TerrainLayers.Num() == 0)
	{
		Messages.Emplace(EVoxelValidationSeverity::Warning, TEXT("Biome has no terrain layers defined"), TEXT("Add at least one layer (e.g. Grass, Dirt, Stone) to TerrainLayers"));
	}

	return Messages;
}
