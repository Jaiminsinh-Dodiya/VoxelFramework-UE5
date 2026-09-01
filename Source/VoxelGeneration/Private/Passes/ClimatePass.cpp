// ClimatePass.cpp

#include "Passes/ClimatePass.h"
#include "VoxelGenerationContext.h"
#include "VoxelNoise.h"

void FClimatePass::Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk)
{
	static const FVoxelClimateConfig DefaultClimate;
	const FVoxelClimateConfig& Climate = Context.Config ? Context.Config->Climate : DefaultClimate;

	for (int32 LocalY = 0; LocalY < Context.ChunkSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < Context.ChunkSize; ++LocalX)
		{
			const FVector2D WorldColumn = Context.LocalToWorldColumn(LocalX, LocalY);

			const float TemperatureNoise = VoxelNoise::Sample2D(
				Context.WorldSeed + Climate.TemperatureSeedOffset,
				WorldColumn.X * Climate.Frequency,
				WorldColumn.Y * Climate.Frequency);

			const float HumidityNoise = VoxelNoise::Sample2D(
				Context.WorldSeed + Climate.HumiditySeedOffset,
				WorldColumn.X * Climate.Frequency,
				WorldColumn.Y * Climate.Frequency);

			FVoxelColumnData& Column = Context.ColumnAt(LocalX, LocalY);
			// Remap noise [-1,1] -> [0,1] to match the range biome definitions author against.
			Column.Temperature = (TemperatureNoise + 1.0f) * 0.5f;
			Column.Humidity = (HumidityNoise + 1.0f) * 0.5f;
		}
	}
}

