// TerrainPass.cpp

#include "Passes/TerrainPass.h"
#include "VoxelGenerationContext.h"
#include "VoxelChunk.h"
#include "VoxelNoise.h"
#include "VoxelBiomeDefinition.h"
#include "VoxelBlockRegistry.h"

namespace
{
	// Fallback block IDs used only when no biome has been assigned to a
	// column at all (BiomePass found zero AvailableBiomes) - a project can
	// legitimately choose not to use biomes yet. This is NOT used when a
	// biome IS assigned; in that case GetResolvedLayerBlockIds is the only
	// source of truth (see Execute below).
	constexpr FVoxelBlockId FallbackStoneId = 1;
	constexpr FVoxelBlockId FallbackDirtId = 2;
	constexpr FVoxelBlockId FallbackGrassId = 3;
}

void FTerrainPass::Execute(FVoxelGenerationContext& Context, FVoxelChunk& Chunk)
{
	const int32 ChunkSize = Context.ChunkSize;
	const int32 ChunkBaseZ = Context.ChunkCoordinate.Z * ChunkSize;

	for (int32 LocalY = 0; LocalY < ChunkSize; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < ChunkSize; ++LocalX)
		{
			const FVector2D WorldColumn = Context.LocalToWorldColumn(LocalX, LocalY);

			const float NoiseValue = VoxelNoise::FractalBrownianMotion2D(
				Context.WorldSeed,
				WorldColumn.X * BaseFrequency,
				WorldColumn.Y * BaseFrequency,
				NoiseOctaves);

			const int32 Height = BaseHeight + FMath::RoundToInt(NoiseValue * HeightAmplitude);
			Context.ColumnAt(LocalX, LocalY).TerrainHeight = Height;

			const UVoxelBiomeDefinition* Biome = Context.ColumnAt(LocalX, LocalY).Biome;

			// Resolved once per column (not per voxel) - GetResolvedLayerBlockIds
			// is an O(1) TMap lookup into a table PrecacheBiomeLayers already
			// built on the Game Thread, so this is safe and cheap to call from
			// this worker thread.
			const TArray<FVoxelBlockId>* ResolvedLayerIds =
				(Biome && Context.BlockRegistry) ? Context.BlockRegistry->GetResolvedLayerBlockIds(Biome) : nullptr;

			for (int32 LocalZ = 0; LocalZ < ChunkSize; ++LocalZ)
			{
				const int32 WorldZ = ChunkBaseZ + LocalZ;
				if (WorldZ > Height)
				{
					continue; // stays air - Chunk starts zeroed
				}

				const int32 DepthBelowSurface = Height - WorldZ;
				FVoxelBlockId BlockId;

				if (Biome && ResolvedLayerIds && ResolvedLayerIds->Num() > 0)
				{
					// Walk the biome's layer thicknesses to find which layer
					// DepthBelowSurface falls into; last layer fills downward.
					int32 Accumulated = 0;
					int32 ChosenLayerIndex = ResolvedLayerIds->Num() - 1; // default: last layer (fills to bedrock)
					for (int32 LayerIndex = 0; LayerIndex < Biome->TerrainLayers.Num(); ++LayerIndex)
					{
						const bool bIsLastLayer = (LayerIndex == Biome->TerrainLayers.Num() - 1);
						if (bIsLastLayer)
						{
							break; // ChosenLayerIndex already defaulted to this
						}

						const int32 Thickness = Biome->TerrainLayers[LayerIndex].ThicknessVoxels;
						if (DepthBelowSurface < Accumulated + Thickness)
						{
							ChosenLayerIndex = LayerIndex;
							break;
						}
						Accumulated += Thickness;
					}

					BlockId = (*ResolvedLayerIds)[ChosenLayerIndex];
				}
				else if (Biome)
				{
					// Biome assigned but its layers were never precached
					// (PrecacheBiomeLayers wasn't called for it) - fall back
					// rather than silently placing air, but log so this
					// doesn't go unnoticed the way the old placeholder did.
					BlockId = DepthBelowSurface == 0 ? FallbackGrassId : (DepthBelowSurface <= 4 ? FallbackDirtId : FallbackStoneId);
				}
				else
				{
					// No biome assigned at all: simple default layering.
					if (DepthBelowSurface == 0)
					{
						BlockId = FallbackGrassId;
					}
					else if (DepthBelowSurface <= 4)
					{
						BlockId = FallbackDirtId;
					}
					else
					{
						BlockId = FallbackStoneId;
					}
				}

				Chunk.SetBlock(LocalX, LocalY, LocalZ, BlockId, /*bIsGenerationWrite=*/true);
			}
		}
	}
}
