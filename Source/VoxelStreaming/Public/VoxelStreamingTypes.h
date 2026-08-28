// VoxelStreamingTypes.h
//
// Purpose:
//   Pure value types and free functions for VoxelStreaming's distance-band
//   classification. Deliberately stateless and unit-testable without
//   touching any subsystem, scheduler, or world.
//
// Thread ownership: N/A (pure functions, value types).
// Dependencies: VoxelCore (FVoxelChunkCoordinate) only.

#pragma once

#include "CoreMinimal.h"
#include "VoxelCoreTypes.h"

/** Distance band a chunk falls into relative to the viewer. Ordered from nearest to farthest. */
enum class EVoxelStreamingBand : uint8
{
	Simulation,   // distance <= SimulationDistance (collision + finalized mesh)
	Render,       // distance <= RenderDistance (visible, rendered)
	Generation,   // distance <= GenerationDistance (data generated + meshed)
	Persistence,  // distance <= PersistenceDistance (kept resident if modified)
	OutOfRange    // distance > PersistenceDistance (eligible for unload)
};

namespace VoxelStreaming
{
	/**
	 * Classifies a Chebyshev distance into a streaming band.
	 * Pure function, no side effects, trivially unit-testable.
	 * Distances must satisfy: Simulation <= Render <= Generation <= Persistence.
	 */
	VOXELSTREAMING_API EVoxelStreamingBand ClassifyChunkDistance(
		int32 ChebyshevDistance,
		int32 SimulationDistance,
		int32 RenderDistance,
		int32 GenerationDistance,
		int32 PersistenceDistance);

	/**
	 * Computes the set of chunk coordinates that should be loaded, given a
	 * viewer chunk position, generation distance, and world height.
	 * Results are sorted by ascending Chebyshev distance to ViewerChunk
	 * (closer chunks first — higher priority for budget-limited loading).
	 * Z is clamped to [0, WorldHeightInChunks).
	 */
	VOXELSTREAMING_API TArray<FVoxelChunkCoordinate> ComputeDesiredCoordinates(
		const FVoxelChunkCoordinate& ViewerChunk,
		int32 GenerationDistance,
		int32 WorldHeightInChunks);
}
