// VoxelMesherTests.cpp
//
// Covers: empty chunk, single voxel, adjacent voxels (hidden internal
// faces + greedy merging), material boundaries, expected triangle/index
// counts, AO behavior, and deterministic output. Perf logging follows the
// existing Voxel.Generation.PerfLog pattern (informational, no hard gate).

#include "Misc/AutomationTest.h"
#include "VoxelMesher.h"
#include "VoxelMeshData.h"
#include "VoxelChunk.h"
#include "VoxelGenerationPipeline.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS

// ---- Empty chunk ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMesherEmptyChunkTest, "Voxel.Meshing.EmptyChunk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMesherEmptyChunkTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(8); // all-air by construction
	const FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, nullptr);

	TestTrue(TEXT("An all-air chunk should produce no mesh data"), MeshData.IsEmpty());
	TestEqual(TEXT("No sections should be created for an empty chunk"), MeshData.Sections.Num(), 0);
	return true;
}

// ---- Single solid voxel: expect exactly 6 unmerged quads (24 verts, 36 indices, 12 tris) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMesherSingleVoxelTest, "Voxel.Meshing.SingleVoxel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMesherSingleVoxelTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(4);
	Chunk.SetBlock(1, 1, 1, /*BlockId=*/5, /*bIsGenerationWrite=*/true); // isolated, surrounded by air on all 6 sides

	const FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, nullptr);

	TestEqual(TEXT("Single isolated voxel should produce exactly 1 section"), MeshData.Sections.Num(), 1);
	TestEqual(TEXT("Single isolated voxel should produce exactly 24 vertices (6 unmerged quads x 4)"), MeshData.Vertices.Num(), 24);
	TestEqual(TEXT("Single isolated voxel should produce exactly 12 triangles (6 faces x 2 tris)"), MeshData.GetTotalTriangleCount(), 12);
	if (MeshData.Sections.Num() > 0)
	{
		TestEqual(TEXT("36 indices total (12 tris x 3)"), MeshData.Sections[0].Indices.Num(), 36);
	}
	return true;
}

// ---- Adjacent same-material voxels: hidden internal face + greedy merge down to 6 quads ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMesherAdjacentSameMaterialTest, "Voxel.Meshing.AdjacentVoxelsMergeSameMaterial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMesherAdjacentSameMaterialTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(4);
	Chunk.SetBlock(0, 0, 0, 5, true);
	Chunk.SetBlock(1, 0, 0, 5, true); // same block ID, adjacent along X

	const FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, nullptr);

	// Without hidden-face removal: 2 voxels x 6 faces = 12 quads.
	// With hidden-face removal only (no merging): 12 - 2 shared faces = 10 quads.
	// With greedy merging on top: the two 1x1 side/top/bottom face pairs
	// each collapse into one 2x1 quad -> 6 quads total. This is the
	// specific number that proves merging happened, not just hidden-face removal.
	TestEqual(TEXT("Adjacent same-material voxels should merge to exactly 6 quads (24 vertices)"), MeshData.Vertices.Num(), 24);
	TestEqual(TEXT("6 merged quads = 12 triangles"), MeshData.GetTotalTriangleCount(), 12);
	TestEqual(TEXT("Same material -> single section"), MeshData.Sections.Num(), 1);
	return true;
}

// ---- Material boundary: adjacent voxels, DIFFERENT block IDs - internal face still hidden, but no merging across materials ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMesherMaterialBoundaryTest, "Voxel.Meshing.MaterialBoundaryPreventsMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMesherMaterialBoundaryTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(4);
	Chunk.SetBlock(0, 0, 0, 5, true);  // block A
	Chunk.SetBlock(1, 0, 0, 9, true);  // block B, different ID, adjacent along X

	const FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, nullptr);

	// The A/B shared face is still hidden (both solid, regardless of material
	// difference - occlusion doesn't care about material). But nothing else
	// can merge across the material boundary, so each voxel keeps its 5
	// remaining exposed faces fully unmerged: 2 x 5 = 10 quads, 40 vertices.
	TestEqual(TEXT("Different-material adjacent voxels: shared face still hidden, no cross-material merge -> 40 vertices"), MeshData.Vertices.Num(), 40);
	TestEqual(TEXT("10 unmerged quads = 20 triangles"), MeshData.GetTotalTriangleCount(), 20);
	TestEqual(TEXT("Different materials (no registry, so raw block IDs) -> 2 sections"), MeshData.Sections.Num(), 2);
	return true;
}

// ---- Ambient occlusion: a neighboring voxel should darken the nearest corner of an exposed face, not the far corner ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMesherAmbientOcclusionTest, "Voxel.Meshing.AmbientOcclusionVariesByProximity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMesherAmbientOcclusionTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(4);
	Chunk.SetBlock(1, 1, 0, 5, true); // "floor" voxel, top face exposed at Z=1
	Chunk.SetBlock(0, 1, 0, 5, true); // "wall" voxel at the same Z=0 layer, adjacent in -X to the floor's near corner
	Chunk.SetBlock(0, 1, 1, 5, true); // wall is 2 voxels tall so its OWN top face sits at Z=2, not Z=1 -
	                                   // otherwise the wall's top face would be coplanar, adjacent, and
	                                   // same-material as the floor's top face and correctly greedy-merge
	                                   // into one quad, eating the exact corner vertex this test inspects.
	                                   // (First version of this test hit exactly that - see commit history:
	                                   // the mesher was right, the single-voxel-tall wall in the test was wrong.)

	const FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, nullptr);

	// Floor's top face is a single unmerged quad (nothing else at Z=1 to
	// merge with) with corners at world XY (1,1) [near the wall] and (2,1)
	// [far from the wall], both at Z=1. Find those two vertices and compare
	// their baked AO intensity (Color.R, since AO is baked as a uniform RGB value).
	const FVoxelMeshVertex* NearCorner = nullptr;
	const FVoxelMeshVertex* FarCorner = nullptr;
	for (const FVoxelMeshVertex& Vertex : MeshData.Vertices)
	{
		if (Vertex.Normal.Z > 0.5f) // top-face vertices only
		{
			if (Vertex.Position.Equals(FVector(1, 1, 1), 0.01f))
			{
				NearCorner = &Vertex;
			}
			else if (Vertex.Position.Equals(FVector(2, 1, 1), 0.01f))
			{
				FarCorner = &Vertex;
			}
		}
	}

	TestNotNull(TEXT("Should find the near-wall top-face corner vertex"), NearCorner);
	TestNotNull(TEXT("Should find the far-from-wall top-face corner vertex"), FarCorner);
	if (NearCorner && FarCorner)
	{
		AddInfo(FString::Printf(TEXT("Near-wall corner AO intensity: %.3f, far corner: %.3f"), NearCorner->Color.R, FarCorner->Color.R));
		TestTrue(TEXT("Corner adjacent to a neighboring solid voxel should be darker (lower intensity) than an open corner"),
			NearCorner->Color.R < FarCorner->Color.R);
		TestEqual(TEXT("Fully open corner should be at full intensity (AO=3/3)"), FarCorner->Color.R, 1.0f);
	}
	return true;
}

// ---- Determinism ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMesherDeterminismTest, "Voxel.Meshing.DeterministicOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMesherDeterminismTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 16;
	FVoxelGenerationPipeline Pipeline;
	FVoxelChunk Chunk(ChunkSize);
	Pipeline.GenerateChunk(2026, FVoxelChunkCoordinate(0, 0, 2), ChunkSize, nullptr, {}, Chunk);

	const FVoxelMeshData MeshA = FVoxelMesher::GenerateMesh(Chunk, nullptr);
	const FVoxelMeshData MeshB = FVoxelMesher::GenerateMesh(Chunk, nullptr);

	TestEqual(TEXT("Same chunk meshed twice: vertex count must match"), MeshA.Vertices.Num(), MeshB.Vertices.Num());
	TestEqual(TEXT("Same chunk meshed twice: section count must match"), MeshA.Sections.Num(), MeshB.Sections.Num());

	bool bAllVerticesMatch = MeshA.Vertices.Num() == MeshB.Vertices.Num();
	if (bAllVerticesMatch)
	{
		for (int32 i = 0; i < MeshA.Vertices.Num(); ++i)
		{
			if (!MeshA.Vertices[i].Position.Equals(MeshB.Vertices[i].Position, 0.001f))
			{
				bAllVerticesMatch = false;
				break;
			}
		}
	}
	TestTrue(TEXT("Vertex positions must be identical across regenerations, in the same order"), bAllVerticesMatch);
	return true;
}

// ---- Perf logging (informational, no hard threshold - see Voxel.Generation.PerfLog for the same pattern) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMesherPerfLogTest, "Voxel.Meshing.PerfLog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelMesherPerfLogTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 32;
	FVoxelGenerationPipeline Pipeline;
	FVoxelChunk Chunk(ChunkSize);
	Pipeline.GenerateChunk(2026, FVoxelChunkCoordinate(0, 0, 2), ChunkSize, nullptr, {}, Chunk);

	const double StartSeconds = FPlatformTime::Seconds();
	const FVoxelMeshData MeshData = FVoxelMesher::GenerateMesh(Chunk, nullptr);
	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

	AddInfo(FString::Printf(TEXT("Meshing a %dx%dx%d generated chunk: %.3f ms, %d vertices, %d triangles, %d sections"),
		ChunkSize, ChunkSize, ChunkSize, ElapsedMs, MeshData.Vertices.Num(), MeshData.GetTotalTriangleCount(), MeshData.Sections.Num()));

	// No pass/fail threshold yet - same reasoning as Voxel.Generation.PerfLog:
	// needs a profiled on-device baseline before a real budget is meaningful.
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
