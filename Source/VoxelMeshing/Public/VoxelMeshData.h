// VoxelMeshData.h
//
// Purpose:
//   Plain CPU-side output of meshing - positions, normals, UVs, vertex
//   colors, material IDs, indices. Per ADR-004, this module has ZERO
//   knowledge of how this data gets to the GPU. No UMeshComponent, no
//   FPrimitiveSceneProxy, no RHI resources, no engine mesh types appear
//   anywhere in VoxelMeshing - VoxelRendering's entire job is consuming
//   this struct and doing the engine-specific upload.
//
// Responsibilities: plain data only.
// Thread ownership: produced on a worker thread (see FVoxelMesher), safe
//   to read from any thread once construction is complete, since nothing
//   here is a UObject or otherwise GC-tracked.
// Dependencies: Core only.

#pragma once

#include "CoreMinimal.h"

/** One mesh vertex. Deliberately NOT deduplicated across quads in the current implementation - see VoxelMesher.cpp comments for why, and Docs/TODO.md for the future shared-vertex-welding note. */
struct FVoxelMeshVertex
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FVector2D UV = FVector2D::ZeroVector;

	/**
	 * Baked lighting data, RGB = ambient occlusion intensity (0=fully
	 * occluded corner, 1=fully open), A unused. Per-vertex, computed
	 * independently of how large the merged quad is - see VoxelMesher.cpp
	 * "corner AO" comments for the algorithm.
	 */
	FLinearColor Color = FLinearColor::White;
};

/** One material's worth of triangles, indexing into FVoxelMeshData::Vertices. */
struct FVoxelMeshSection
{
	/** Resolved from UVoxelBlockDefinition::MaterialLayerIndex when a block registry is available, otherwise falls back to the raw FVoxelBlockId - see FVoxelMesher::GenerateMesh. */
	int32 MaterialId = 0;

	/** Triangle list, 3 indices per triangle, indexing into the owning FVoxelMeshData::Vertices array. */
	TArray<uint32> Indices;
};

struct FVoxelMeshData
{
	TArray<FVoxelMeshVertex> Vertices;
	TArray<FVoxelMeshSection> Sections;

	bool IsEmpty() const { return Vertices.Num() == 0; }

	int32 GetTotalTriangleCount() const
	{
		int32 Total = 0;
		for (const FVoxelMeshSection& Section : Sections)
		{
			Total += Section.Indices.Num() / 3;
		}
		return Total;
	}
};
