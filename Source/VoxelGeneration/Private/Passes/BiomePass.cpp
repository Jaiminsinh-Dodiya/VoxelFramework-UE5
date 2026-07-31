// BiomePass.cpp

#include "Passes/BiomePass.h"
#include "VoxelGenerationContext.h"
#include "VoxelBiomeDefinition.h"

void FBiomePass::Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk)
{
	if (Context.AvailableBiomes.Num() == 0)
	{
		return; // no biomes configured - TerrainPass falls back to its default layering
	}

	for (int32 LocalY = 0; LocalY < Context.ChunkSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < Context.ChunkSize; ++LocalX)
		{
			FVoxelColumnData& Column = Context.ColumnAt(LocalX, LocalY);

			const UVoxelBiomeDefinition* SelectedBiome = nullptr;
			for (const UVoxelBiomeDefinition* Biome : Context.AvailableBiomes)
			{
				if (!Biome)
				{
					continue;
				}

				const bool bTemperatureMatch = Column.Temperature >= Biome->TemperatureRange.X && Column.Temperature <= Biome->TemperatureRange.Y;
				const bool bHumidityMatch = Column.Humidity >= Biome->HumidityRange.X && Column.Humidity <= Biome->HumidityRange.Y;

				if (bTemperatureMatch && bHumidityMatch)
				{
					SelectedBiome = Biome;
					break; // first match wins - project defines non-overlapping ranges, or accepts priority-by-list-order
				}
			}

			// Fall back to the first configured biome if none of the ranges
			// matched, so a column is never left without a biome once biomes
			// are in use at all (avoids a visible seam of "default" terrain).
			Column.Biome = SelectedBiome ? SelectedBiome : Context.AvailableBiomes[0];
		}
	}
}
