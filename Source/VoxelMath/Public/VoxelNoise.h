// VoxelNoise.h
//
// Purpose:
//   Pure, deterministic noise functions used by the (future) VoxelGeneration
//   pipeline. Nothing here touches chunk storage or UObjects - every
//   function is a pure mapping from (seed, coordinates) to a value, which
//   is what makes generation reproducible from a seed alone (ADR-005).
//
// Responsibilities: 2D/3D noise sampling + fractal (octave) combination.
// Thread ownership: fully reentrant, safe to call from any worker thread
//   with no synchronization - no shared mutable state exists.
// Dependencies: Core, VoxelCore (for FVoxelChunkCoordinate convenience
//   overloads only).
// Performance notes: called per-voxel during generation, so kept branch-
//   light and allocation-free. If profiling shows this is hot, consider a
//   SIMD batch-sampling entry point that returns N values at once instead
//   of changing the per-call signature.

#pragma once

#include "CoreMinimal.h"

namespace VoxelNoise
{
	/** Deterministic 2D value noise in [-1, 1] for a given integer world seed. */
	VOXELMATH_API float Sample2D(int32 Seed, float X, float Y);

	/** Deterministic 3D value noise in [-1, 1] for a given integer world seed. */
	VOXELMATH_API float Sample3D(int32 Seed, float X, float Y, float Z);

	/**
	 * Fractal Brownian Motion: sums Octaves layers of Sample2D at increasing
	 * frequency and decreasing amplitude. Standard terrain-height building block.
	 */
	VOXELMATH_API float FractalBrownianMotion2D(int32 Seed, float X, float Y, int32 Octaves, float Lacunarity = 2.0f, float Gain = 0.5f);

	/** 3D counterpart, used for caves/ore density fields. */
	VOXELMATH_API float FractalBrownianMotion3D(int32 Seed, float X, float Y, float Z, int32 Octaves, float Lacunarity = 2.0f, float Gain = 0.5f);
}
