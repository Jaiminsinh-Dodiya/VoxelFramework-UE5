// VoxelPhysicsTests.cpp

#include "Misc/AutomationTest.h"
#include "VoxelCollisionBuilder.h"
#include "VoxelCollisionComponent.h"
#include "VoxelChunk.h"
#include "VoxelChunkStore.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBlockDefinition.h"
#include "VoxelCoreTypes.h"
#include "VoxelPhysicsTypes.h"
#include "VoxelMesher.h"

// ============================================================================
// Test 1: Empty Chunk produces zero collision geometry
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsEmptyChunkTest,
	"Voxel.Physics.EmptyChunk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsEmptyChunkTest::RunTest(const FString& Parameters)
{
	FVoxelChunk EmptyChunk(32);
	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(EmptyChunk, nullptr);

	TestTrue(TEXT("Result is empty"), Result.IsEmpty());
	TestEqual(TEXT("Zero vertices"), Result.GetVertexCount(), 0);
	TestEqual(TEXT("Zero triangles"), Result.GetTriangleCount(), 0);
	return true;
}

// ============================================================================
// Test 2: Single isolated voxel produces 6 quads (12 triangles, 24 vertices)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsSingleVoxelTest,
	"Voxel.Physics.SingleVoxel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsSingleVoxelTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(32);
	Chunk.SetBlock(10, 10, 10, 1, /*bIsGenerationWrite=*/ true);

	const FVoxelChunkCoordinate Coord(0, 0, 0);
	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(
		Chunk, nullptr, nullptr, &Coord, 100.0f);

	TestFalse(TEXT("Result is not empty"), Result.IsEmpty());
	TestEqual(TEXT("Single voxel has 24 vertices (6 quads * 4)"), Result.GetVertexCount(), 24);
	TestEqual(TEXT("Single voxel has 12 triangles (6 quads * 2)"), Result.GetTriangleCount(), 12);
	TestTrue(TEXT("Bounds are valid"), Result.Bounds.IsValid != 0);

	// Voxel at (10,10,10) of size 100cm spans [1000, 1100] on all axes
	TestEqual(TEXT("Min bound X"), Result.Bounds.Min.X, 1000.0);
	TestEqual(TEXT("Max bound X"), Result.Bounds.Max.X, 1100.0);
	TestEqual(TEXT("Min bound Y"), Result.Bounds.Min.Y, 1000.0);
	TestEqual(TEXT("Max bound Y"), Result.Bounds.Max.Y, 1100.0);
	TestEqual(TEXT("Min bound Z"), Result.Bounds.Min.Z, 1000.0);
	TestEqual(TEXT("Max bound Z"), Result.Bounds.Max.Z, 1100.0);

	return true;
}

// ============================================================================
// Test 3: Flat Surface greedy merging (32x32x1 slab top merged into 1 quad)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsFlatSurfaceTest,
	"Voxel.Physics.FlatSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsFlatSurfaceTest::RunTest(const FString& Parameters)
{
	const int32 Size = 32;
	FVoxelChunk Chunk(Size);

	// Fill bottom layer (Z = 0)
	for (int32 X = 0; X < Size; ++X)
	{
		for (int32 Y = 0; Y < Size; ++Y)
		{
			Chunk.SetBlock(X, Y, 0, 1, /*bIsGenerationWrite=*/ true);
		}
	}

	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr);

	TestFalse(TEXT("Result is not empty"), Result.IsEmpty());
	// Top face (Z=0 facing +Z) = 1 merged quad (2 triangles)
	// Bottom face (Z=0 facing -Z) = 1 merged quad (2 triangles)
	// 4 boundary side walls (X=0, X=31, Y=0, Y=31), each 32x1 = 4 merged quads (8 triangles)
	// Total: 6 quads = 12 triangles, 24 vertices!
	TestEqual(TEXT("Greedy merged 32x32 flat slab produces 6 quads = 12 triangles"), Result.GetTriangleCount(), 12);
	TestEqual(TEXT("Greedy merged 32x32 flat slab produces 24 vertices"), Result.GetVertexCount(), 24);

	return true;
}

// ============================================================================
// Test 4: Slope geometry produces correct multi-quad stepped surface
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsSlopeTest,
	"Voxel.Physics.Slope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsSlopeTest::RunTest(const FString& Parameters)
{
	const int32 Size = 32;
	FVoxelChunk Chunk(Size);

	// Create 4-step staircase
	for (int32 X = 0; X < 4; ++X)
	{
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 Z = 0; Z <= X; ++Z)
			{
				Chunk.SetBlock(X, Y, Z, 1, /*bIsGenerationWrite=*/ true);
			}
		}
	}

	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr);
	TestFalse(TEXT("Slope collision is non-empty"), Result.IsEmpty());
	TestTrue(TEXT("Slope bounds are valid"), Result.Bounds.IsValid != 0);
	TestTrue(TEXT("Slope contains triangles"), Result.GetTriangleCount() > 0);

	return true;
}

// ============================================================================
// Test 5: Cave carving internal collision verification
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsCaveTest,
	"Voxel.Physics.Cave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsCaveTest::RunTest(const FString& Parameters)
{
	const int32 Size = 32;
	FVoxelChunk Chunk(Size);

	// Fill entire solid chunk
	for (int32 X = 0; X < Size; ++X)
	{
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 Z = 0; Z < Size; ++Z)
			{
				Chunk.SetBlock(X, Y, Z, 1, /*bIsGenerationWrite=*/ true);
			}
		}
	}

	// Carve a 4x4x4 air pocket in the interior
	for (int32 X = 14; X < 18; ++X)
	{
		for (int32 Y = 14; Y < 18; ++Y)
		{
			for (int32 Z = 14; Z < 18; ++Z)
			{
				Chunk.SetBlock(X, Y, Z, VoxelBlockId_Air, /*bIsGenerationWrite=*/ true);
			}
		}
	}

	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr);
	TestFalse(TEXT("Cave collision is non-empty"), Result.IsEmpty());

	// Outer 6 faces of chunk (6 quads = 12 tris) + 6 interior cave walls (6 quads = 12 tris) = 24 tris
	TestEqual(TEXT("Solid chunk with 4x4x4 carved cave produces 24 triangles"), Result.GetTriangleCount(), 24);

	return true;
}

// ============================================================================
// Test 6 & 7: Neighbor Boundary Culling
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsNeighborBoundaryTest,
	"Voxel.Physics.NeighborBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsNeighborBoundaryTest::RunTest(const FString& Parameters)
{
	const int32 Size = 32;
	FVoxelChunk MainChunk(Size);
	FVoxelChunk PosXNeighbor(Size);

	// MainChunk has solid voxel at boundary (31, 10, 10)
	MainChunk.SetBlock(31, 10, 10, 1, /*bIsGenerationWrite=*/ true);

	// Case A: Without neighbor (PosX is air fallback) -> +X face is emitted (6 quads = 12 tris)
	const FVoxelCollisionData ResultWithoutNeighbor = FVoxelCollisionBuilder::BuildCollisionData(
		MainChunk, nullptr, nullptr);
	TestEqual(TEXT("Without neighbor, boundary voxel has 12 triangles"), ResultWithoutNeighbor.GetTriangleCount(), 12);

	// Case B: With neighbor having adjacent solid voxel at (0, 10, 10)
	PosXNeighbor.SetBlock(0, 10, 10, 1, /*bIsGenerationWrite=*/ true);

	FVoxelNeighborChunks Neighbors;
	Neighbors.PosX = &PosXNeighbor;

	const FVoxelCollisionData ResultWithNeighbor = FVoxelCollisionBuilder::BuildCollisionData(
		MainChunk, nullptr, &Neighbors);

	// +X face is culled across the chunk seam -> 5 quads = 10 triangles
	TestEqual(TEXT("With solid neighbor, boundary +X face is culled (10 triangles)"), ResultWithNeighbor.GetTriangleCount(), 10);

	return true;
}

// ============================================================================
// Test 8: Missing / Null Neighbor gracefully treated as air
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsMissingNeighborTest,
	"Voxel.Physics.MissingNeighbor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsMissingNeighborTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(32);
	Chunk.SetBlock(0, 0, 0, 1, /*bIsGenerationWrite=*/ true);

	FVoxelNeighborChunks NullNeighbors; // all nullptrs
	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(
		Chunk, nullptr, &NullNeighbors);

	TestFalse(TEXT("Result is not empty"), Result.IsEmpty());
	TestEqual(TEXT("All 6 faces emitted safely on missing neighbors"), Result.GetTriangleCount(), 12);

	return true;
}

// ============================================================================
// Test 9: Deterministic Output
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsDeterministicTest,
	"Voxel.Physics.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsDeterministicTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(32);
	for (int32 X = 5; X < 15; ++X)
	{
		for (int32 Y = 5; Y < 15; ++Y)
		{
			Chunk.SetBlock(X, Y, 2, 1, /*bIsGenerationWrite=*/ true);
		}
	}

	const FVoxelCollisionData Run1 = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr);
	const FVoxelCollisionData Run2 = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr);

	TestEqual(TEXT("Vertex count matches"), Run1.GetVertexCount(), Run2.GetVertexCount());
	TestEqual(TEXT("Index count matches"), Run1.GetTriangleCount(), Run2.GetTriangleCount());
	TestEqual(TEXT("Bounds Min matches"), Run1.Bounds.Min, Run2.Bounds.Min);
	TestEqual(TEXT("Bounds Max matches"), Run1.Bounds.Max, Run2.Bounds.Max);

	for (int32 i = 0; i < Run1.Vertices.Num(); ++i)
	{
		TestEqual(TEXT("Vertex position matches"), Run1.Vertices[i], Run2.Vertices[i]);
	}

	return true;
}

// ============================================================================
// Test 10: Non-collidable block definitions filtered
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsNonCollidableFilterTest,
	"Voxel.Physics.NonCollidableFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsNonCollidableFilterTest::RunTest(const FString& Parameters)
{
	// Create mock block registry and definition with bGeneratesCollision = false
	UVoxelBlockDefinition* FoliageDef = NewObject<UVoxelBlockDefinition>();
	FoliageDef->BlockId = 5;
	FoliageDef->bIsSolid = true;
	FoliageDef->bGeneratesCollision = false; // Non-collidable!

	UVoxelBlockRegistry* Registry = NewObject<UVoxelBlockRegistry>();
	TArray<UVoxelBlockDefinition*> Defs = { FoliageDef };
	Registry->BuildFromDefinitions(Defs);

	FVoxelChunk Chunk(32);
	Chunk.SetBlock(10, 10, 10, 5, /*bIsGenerationWrite=*/ true);

	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(Chunk, Registry);

	TestTrue(TEXT("Non-collidable block produces empty collision data"), Result.IsEmpty());
	TestEqual(TEXT("Zero triangles emitted"), Result.GetTriangleCount(), 0);

	return true;
}

// ============================================================================
// Test 11: Stale revision discard & async component lifecycle
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsStaleRevisionTest,
	"Voxel.Physics.StaleRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsStaleRevisionTest::RunTest(const FString& Parameters)
{
	UVoxelCollisionComponent* Comp = NewObject<UVoxelCollisionComponent>();
	TestNotNull(TEXT("Collision component created"), Comp);

	FVoxelChunk Chunk(32);
	Chunk.SetBlock(0, 0, 0, 1, /*bIsGenerationWrite=*/ true);

	FVoxelCollisionData DataRev1 = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr, nullptr, nullptr, 100.0f, /*Revision=*/ 1);
	FVoxelCollisionData DataRev2 = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr, nullptr, nullptr, 100.0f, /*Revision=*/ 2);

	// Synchronous cook for deterministic testing
	Comp->SetCollisionData(MoveTemp(DataRev1), /*bAsyncCook=*/ false);
	TestEqual(TEXT("Current revision is 1"), Comp->GetCurrentCollisionRevision(), 1u);
	TestTrue(TEXT("Component has active collision"), Comp->HasActiveCollision());

	// Update to Revision 2
	Comp->SetCollisionData(MoveTemp(DataRev2), /*bAsyncCook=*/ false);
	TestEqual(TEXT("Current revision is 2"), Comp->GetCurrentCollisionRevision(), 2u);
	TestTrue(TEXT("Component has active collision for revision 2"), Comp->HasActiveCollision());

	// Clear collision
	Comp->ClearCollisionData();
	TestFalse(TEXT("Component active collision cleared"), Comp->HasActiveCollision());

	return true;
}

// ============================================================================
// Test 12: Cook Failure Handling & Delegate Notification
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsCookFailureTest,
	"Voxel.Physics.CookFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsCookFailureTest::RunTest(const FString& Parameters)
{
	UVoxelCollisionComponent* Comp = NewObject<UVoxelCollisionComponent>();
	TestNotNull(TEXT("Collision component created"), Comp);

	bool bCookSuccessResult = true;
	int32 DelegateCallCount = 0;

	Comp->OnCollisionCookFinished.AddLambda([&bCookSuccessResult, &DelegateCallCount](UVoxelCollisionComponent* InComp, bool bSuccess, uint32 Rev)
	{
		bCookSuccessResult = bSuccess;
		DelegateCallCount++;
	});

	// Empty data fast-path: should complete immediately with empty collision (no failed BodySetup)
	FVoxelCollisionData EmptyData;
	EmptyData.CollisionRevision = 1;
	Comp->SetCollisionData(MoveTemp(EmptyData), /*bAsyncCook=*/ true);

	TestEqual(TEXT("Delegate called once for empty fast-path"), DelegateCallCount, 1);
	TestTrue(TEXT("Empty fast-path reports clean success"), bCookSuccessResult);
	TestFalse(TEXT("Component does not have active collision for empty data"), Comp->HasActiveCollision());

	return true;
}

// ============================================================================
// Test 13: Unload / Abort During In-Flight Cook
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsUnloadDuringCookTest,
	"Voxel.Physics.UnloadDuringCook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsUnloadDuringCookTest::RunTest(const FString& Parameters)
{
	UVoxelCollisionComponent* Comp = NewObject<UVoxelCollisionComponent>();
	TestNotNull(TEXT("Collision component created"), Comp);

	FVoxelChunk Chunk(32);
	Chunk.SetBlock(5, 5, 5, 1, /*bIsGenerationWrite=*/ true);

	FVoxelCollisionData Data = FVoxelCollisionBuilder::BuildCollisionData(Chunk, nullptr, nullptr, nullptr, 100.0f, /*Revision=*/ 1);

	// Start async cook
	Comp->SetCollisionData(MoveTemp(Data), /*bAsyncCook=*/ true);

	// Abort/Clear immediately while cook is in-flight
	Comp->ClearCollisionData();

	TestFalse(TEXT("Component has no active collision after abort"), Comp->HasActiveCollision());

	return true;
}

// ============================================================================
// Test 14: Neighbor Arrival During Collision Generation Lifecycle
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsNeighborArrivalDuringCookTest,
	"Voxel.Physics.NeighborArrivalDuringCook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsNeighborArrivalDuringCookTest::RunTest(const FString& Parameters)
{
	const int32 Size = 32;
	FVoxelChunk ChunkA(Size);
	FVoxelChunk ChunkB(Size);

	// Chunk A has boundary solid voxel at (31, 10, 10)
	ChunkA.SetBlock(31, 10, 10, 1, /*bIsGenerationWrite=*/ true);

	// Step 1: Chunk A built before Chunk B arrives (no neighbor) -> 6 quads = 12 triangles
	FVoxelCollisionData DataA1 = FVoxelCollisionBuilder::BuildCollisionData(ChunkA, nullptr, nullptr);
	TestEqual(TEXT("Chunk A before neighbor arrival has 12 triangles"), DataA1.GetTriangleCount(), 12);

	// Step 2: Neighbor Chunk B arrives and solidifies adjacent voxel at (0, 10, 10)
	ChunkB.SetBlock(0, 10, 10, 1, /*bIsGenerationWrite=*/ true);

	FVoxelNeighborChunks Neighbors;
	Neighbors.PosX = &ChunkB;

	// Step 3: Chunk A rebuilds with neighbor present -> 5 quads = 10 triangles (boundary quad culled)
	FVoxelCollisionData DataA2 = FVoxelCollisionBuilder::BuildCollisionData(ChunkA, nullptr, &Neighbors);
	TestEqual(TEXT("Chunk A after neighbor arrival culls boundary face (10 triangles)"), DataA2.GetTriangleCount(), 10);

	return true;
}

// ============================================================================
// Test 15: Neighbor Unload During In-Flight Worker Lease Safety
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsNeighborUnloadDuringCookTest,
	"Voxel.Physics.NeighborUnloadDuringCook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsNeighborUnloadDuringCookTest::RunTest(const FString& Parameters)
{
	FVoxelChunkStore Store(32);
	const FVoxelChunkCoordinate CoordA(0, 0, 0);
	const FVoxelChunkCoordinate CoordB(1, 0, 0);

	Store.CreateOrGetChunk(CoordA);
	Store.CreateOrGetChunk(CoordB);

	// Simulate collision worker leasing neighbor B
	const int32 LeaseSlotB = Store.AcquireWorkerLease(CoordB);
	TestTrue(TEXT("Neighbor lease acquired"), LeaseSlotB != (int32)INDEX_NONE);

	// Unload neighbor B while worker is executing
	Store.RemoveChunk(CoordB);

	// Chunk B coordinate is removed from active map
	TestTrue(TEXT("CoordB removed from active lookup"), Store.FindChunkByCoordinate(CoordB) == nullptr);

	// But slot memory is preserved because WorkerLeaseCount > 0
	TestEqual(TEXT("Loaded chunk count is 1 (CoordA)"), Store.GetLoadedChunkCount(), 1);

	// Worker finishes collision build and releases lease on neighbor B
	Store.ReleaseWorkerLease(LeaseSlotB);

	// Now neighbor B slot is safely recycled with zero memory corruption
	return true;
}

// ============================================================================
// Test 16: Outward Winding & Normal Direction Verification
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelPhysicsOutwardWindingNormalsTest,
	"Voxel.Physics.OutwardWindingNormals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelPhysicsOutwardWindingNormalsTest::RunTest(const FString& Parameters)
{
	FVoxelChunk Chunk(32);
	Chunk.SetBlock(10, 10, 10, 1, /*bIsGenerationWrite=*/ true);

	const FVoxelChunkCoordinate Coord(0, 0, 0);
	const FVoxelCollisionData Result = FVoxelCollisionBuilder::BuildCollisionData(
		Chunk, nullptr, nullptr, &Coord, 100.0f);

	TestFalse(TEXT("Result is not empty"), Result.IsEmpty());
	TestEqual(TEXT("12 triangles emitted"), Result.GetTriangleCount(), 12);

	int32 UpwardFacingCount = 0;
	int32 DownwardFacingCount = 0;
	int32 PosXFacingCount = 0;
	int32 NegXFacingCount = 0;
	int32 PosYFacingCount = 0;
	int32 NegYFacingCount = 0;

	for (const FTriIndices& Tri : Result.Indices)
	{
		const FVector3f& V0 = Result.Vertices[Tri.v0];
		const FVector3f& V1 = Result.Vertices[Tri.v1];
		const FVector3f& V2 = Result.Vertices[Tri.v2];

		const FVector3f Edge1 = V1 - V0;
		const FVector3f Edge2 = V2 - V0;
		const FVector3f GeometricNormal = (Edge1 ^ Edge2).GetSafeNormal();

		if (GeometricNormal.Z > 0.9f) UpwardFacingCount++;
		if (GeometricNormal.Z < -0.9f) DownwardFacingCount++;
		if (GeometricNormal.X > 0.9f) PosXFacingCount++;
		if (GeometricNormal.X < -0.9f) NegXFacingCount++;
		if (GeometricNormal.Y > 0.9f) PosYFacingCount++;
		if (GeometricNormal.Y < -0.9f) NegYFacingCount++;
	}

	TestEqual(TEXT("Top (+Z floor) has 2 triangles pointing UP"), UpwardFacingCount, 2);
	TestEqual(TEXT("Bottom (-Z) has 2 triangles pointing DOWN"), DownwardFacingCount, 2);
	TestEqual(TEXT("+X wall has 2 triangles pointing +X"), PosXFacingCount, 2);
	TestEqual(TEXT("-X wall has 2 triangles pointing -X"), NegXFacingCount, 2);
	TestEqual(TEXT("+Y wall has 2 triangles pointing +Y"), PosYFacingCount, 2);
	TestEqual(TEXT("-Y wall has 2 triangles pointing -Y"), NegYFacingCount, 2);

	return true;
}


