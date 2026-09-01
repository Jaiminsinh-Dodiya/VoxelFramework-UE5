// CavePass.cpp

#include "Passes/CavePass.h"
#include "VoxelGenerationContext.h"
#include "VoxelChunk.h"
#include "VoxelNoise.h"

void FCavePass::Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk)
{
	static const FVoxelCaveConfig DefaultCaves;
	const FVoxelCaveConfig& Caves = Context.Config ? Context.Config->Caves : DefaultCaves;

	if (!Caves.bEnabled)
	{
		return;
	}

	const int32 ChunkSize = Context.ChunkSize;
	const int32 ChunkBaseZ = Context.ChunkCoordinate.Z * ChunkSize;

	for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
		{
			const FVoxelColumnData& Column = Context.ColumnAt(LocalX, LocalY);
			const FVector2D WorldColumn = Context.LocalToWorldColumn(LocalX, LocalY);

			for (int32 LocalZ = 0; LocalZ < ChunkSize; ++LocalZ)
			{
				const int32 WorldZ = ChunkBaseZ + LocalZ;

				// Surface protection: never carve at or above (TerrainHeight -
				// SurfaceProtectionDepth). Also nothing to carve above the
				// surface anyway - it's already air from TerrainPass.
				if (WorldZ > Column.TerrainHeight - Caves.SurfaceProtectionDepth)
				{
					continue;
				}

				const FVoxelBlockId CurrentBlock = Chunk.GetBlock(LocalX, LocalY, LocalZ);
				if (CurrentBlock == VoxelBlockId_Air)
				{
					continue; // nothing to carve
				}

				const float Density = VoxelNoise::FractalBrownianMotion3D(
					Context.WorldSeed + Caves.CaveSeedOffset,
					WorldColumn.X * Caves.DensityFrequency,
					WorldColumn.Y * Caves.DensityFrequency,
					WorldZ * Caves.DensityFrequency,
					Caves.NoiseOctaves,
					Caves.Lacunarity,
					Caves.Persistence);

				// FractalBrownianMotion3D returns roughly [-1,1]; remap to
				// [0,1] so CarveThreshold reads naturally as "carve the top
				// X% densest pockets" rather than requiring a negative constant.
				const float RemappedDensity = (Density + 1.0f) * 0.5f;

				if (RemappedDensity > Caves.CarveThreshold)
				{
					Chunk.SetBlock(LocalX, LocalY, LocalZ, VoxelBlockId_Air, /*bIsGenerationWrite=*/true);
				}
			}
		}
	}
}

