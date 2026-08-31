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

/** One mesh vertex. Deliberately NOT deduplicated across quads in the current implementation. */
struct FVoxelMeshVertex
{
	FVector3f Position = FVector3f::ZeroVector;
	FVector3f Normal = FVector3f::UpVector;
	FVector2f UV = FVector2f::ZeroVector;

	/**
	 * Baked lighting data: RGB = ambient occlusion intensity (0=fully occluded corner, 255=fully open),
	 * A = 255 (unused).
	 */
	FColor Color = FColor::White;
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
	FBox Bounds = FBox(ForceInit);

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
