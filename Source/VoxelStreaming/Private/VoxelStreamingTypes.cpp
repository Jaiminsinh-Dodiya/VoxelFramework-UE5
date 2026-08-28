// VoxelStreamingTypes.cpp

#include "VoxelStreamingTypes.h"

EVoxelStreamingBand VoxelStreaming::ClassifyChunkDistance(
	int32 ChebyshevDistance,
	int32 SimulationDistance,
	int32 RenderDistance,
	int32 GenerationDistance,
	int32 PersistenceDistance)
{
	if (ChebyshevDistance <= SimulationDistance) return EVoxelStreamingBand::Simulation;
	if (ChebyshevDistance <= RenderDistance)     return EVoxelStreamingBand::Render;
	if (ChebyshevDistance <= GenerationDistance) return EVoxelStreamingBand::Generation;
	if (ChebyshevDistance <= PersistenceDistance) return EVoxelStreamingBand::Persistence;
	return EVoxelStreamingBand::OutOfRange;
}

TArray<FVoxelChunkCoordinate> VoxelStreaming::ComputeDesiredCoordinates(
	const FVoxelChunkCoordinate& ViewerChunk,
	int32 GenerationDistance,
	int32 WorldHeightInChunks)
{
	TArray<FVoxelChunkCoordinate> Result;

	// Reserve approximate capacity: (2*d+1)^2 * h
	const int32 Side = 2 * GenerationDistance + 1;
	Result.Reserve(Side * Side * WorldHeightInChunks);

	for (int32 DX = -GenerationDistance; DX <= GenerationDistance; ++DX)
	{
		for (int32 DY = -GenerationDistance; DY <= GenerationDistance; ++DY)
		{
			for (int32 CZ = 0; CZ < WorldHeightInChunks; ++CZ)
			{
				const FVoxelChunkCoordinate Candidate(ViewerChunk.X + DX, ViewerChunk.Y + DY, CZ);
				const int32 Dist = ViewerChunk.ChebyshevDistanceTo(Candidate);
				if (Dist <= GenerationDistance)
				{
					Result.Add(Candidate);
				}
			}
		}
	}

	// Sort by ascending distance — closer chunks get requested first (budget priority).
	const FVoxelChunkCoordinate SortCenter = ViewerChunk;
	Result.Sort([SortCenter](const FVoxelChunkCoordinate& A, const FVoxelChunkCoordinate& B)
	{
		return SortCenter.ChebyshevDistanceTo(A) < SortCenter.ChebyshevDistanceTo(B);
	});

	return Result;
}
