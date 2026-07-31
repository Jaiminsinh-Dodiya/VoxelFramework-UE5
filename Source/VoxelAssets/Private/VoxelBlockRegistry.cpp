// VoxelBlockRegistry.cpp

#include "VoxelBlockRegistry.h"
#include "VoxelBiomeDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelAssets, Log, All);

void UVoxelBlockRegistry::BuildFromDefinitions(const TArray<UVoxelBlockDefinition*>& Definitions)
{
	int32 MaxId = 0;
	for (const UVoxelBlockDefinition* Definition : Definitions)
	{
		if (Definition)
		{
			MaxId = FMath::Max(MaxId, Definition->BlockId);
		}
	}

	DefinitionsById.Reset();
	DefinitionsById.SetNumZeroed(MaxId + 1); // index 0 stays nullptr = air

	for (UVoxelBlockDefinition* Definition : Definitions)
	{
		if (!Definition)
		{
			continue;
		}

		if (Definition->BlockId <= 0)
		{
			UE_LOG(LogVoxelAssets, Warning, TEXT("Block definition '%s' has invalid BlockId %d (must be > 0, 0 is reserved for air) - skipped."),
				*Definition->GetName(), Definition->BlockId);
			continue;
		}

		if (DefinitionsById[Definition->BlockId] != nullptr)
		{
			UE_LOG(LogVoxelAssets, Error, TEXT("Duplicate BlockId %d: '%s' conflicts with '%s' - keeping the first one registered."),
				Definition->BlockId, *Definition->GetName(), *DefinitionsById[Definition->BlockId]->GetName());
			continue;
		}

		DefinitionsById[Definition->BlockId] = Definition;
	}
}

const UVoxelBlockDefinition* UVoxelBlockRegistry::FindDefinition(FVoxelBlockId BlockId) const
{
	if (BlockId == VoxelBlockId_Air || !DefinitionsById.IsValidIndex(BlockId))
	{
		return nullptr;
	}
	return DefinitionsById[BlockId];
}

bool UVoxelBlockRegistry::IsSolid(FVoxelBlockId BlockId) const
{
	const UVoxelBlockDefinition* Definition = FindDefinition(BlockId);
	return Definition ? Definition->bIsSolid : false;
}

void UVoxelBlockRegistry::PrecacheBiomeLayers(const TArray<UVoxelBiomeDefinition*>& Biomes)
{
	check(IsInGameThread()); // LoadSynchronous below requires this - see header comment

	for (const UVoxelBiomeDefinition* Biome : Biomes)
	{
		if (!Biome)
		{
			continue;
		}

		TArray<FVoxelBlockId> ResolvedIds;
		ResolvedIds.Reserve(Biome->TerrainLayers.Num());

		for (const FVoxelTerrainLayer& Layer : Biome->TerrainLayers)
		{
			const UVoxelBlockDefinition* ResolvedBlock = Layer.Block.LoadSynchronous();
			if (!ResolvedBlock)
			{
				UE_LOG(LogVoxelAssets, Warning, TEXT("Biome '%s' has a terrain layer with an unset or unresolvable block reference - VoxelBlockId_Air will be used for that layer."),
					*Biome->GetName());
				ResolvedIds.Add(VoxelBlockId_Air);
				continue;
			}

			ResolvedIds.Add(static_cast<FVoxelBlockId>(ResolvedBlock->BlockId));
		}

		ResolvedBiomeLayers.Add(Biome, MoveTemp(ResolvedIds));
	}
}

const TArray<FVoxelBlockId>* UVoxelBlockRegistry::GetResolvedLayerBlockIds(const UVoxelBiomeDefinition* Biome) const
{
	return ResolvedBiomeLayers.Find(Biome);
}
