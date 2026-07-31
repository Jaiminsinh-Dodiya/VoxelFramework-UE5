// VoxelCavePassTests.cpp
//
// Covers the five properties called out for CavePass specifically:
// determinism (via full-chunk hash), air ratio (logged, not hard-gated
// yet - see note below), surface protection, chunk boundary continuity,
// and generation time logging.

#include "Misc/AutomationTest.h"
#include "VoxelGenerationPipeline.h"
#include "VoxelGenerationContext.h"
#include "VoxelChunk.h"
#include "HAL/PlatformTime.h"
#include "Passes/ClimatePass.h"
#include "Passes/BiomePass.h"
#include "Passes/TerrainPass.h"
#include "Passes/CavePass.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoxelCaveTestHelpers
{
	// Simple CRC32 over the full block buffer - "hash the chunk" for the
	// determinism check. Not exposed on FVoxelChunk itself since nothing
	// outside tests needs it yet; add a real ComputeHash() there if a
	// second caller shows up (e.g. VoxelSerialization diff-verification).
	static uint32 HashChunk(const FVoxelChunk& Chunk)
	{
		TArray<FVoxelBlockId> Flat;
		const int32 Size = Chunk.GetSize();
		Flat.Reserve(Size * Size * Size);
		for (int32 Z = 0; Z < Size; ++Z)
			for (int32 Y = 0; Y < Size; ++Y)
				for (int32 X = 0; X < Size; ++X)
					Flat.Add(Chunk.GetBlock(X, Y, Z));

		return FCrc::MemCrc32(Flat.GetData(), Flat.Num() * sizeof(FVoxelBlockId));
	}

	static float ComputeAirRatio(const FVoxelChunk& Chunk)
	{
		const int32 Size = Chunk.GetSize();
		int32 AirCount = 0;
		const int32 Total = Size * Size * Size;
		for (int32 Z = 0; Z < Size; ++Z)
			for (int32 Y = 0; Y < Size; ++Y)
				for (int32 X = 0; X < Size; ++X)
					if (Chunk.GetBlock(X, Y, Z) == VoxelBlockId_Air)
						++AirCount;

		return Total > 0 ? static_cast<float>(AirCount) / Total : 0.0f;
	}
}

// ---- 1. Determinism (full-chunk hash, stronger than the per-voxel loop in DeterministicFromSeed) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCaveDeterminismTest, "Voxel.Generation.Cave.DeterministicHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelCaveDeterminismTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 16;
	const int32 Seed = 999;
	const FVoxelChunkCoordinate Coord(3, 2, 0);

	FVoxelGenerationPipeline Pipeline;

	FVoxelChunk ChunkA(ChunkSize);
	FVoxelChunk ChunkB(ChunkSize);
	Pipeline.GenerateChunk(Seed, Coord, ChunkSize, nullptr, {}, ChunkA);
	Pipeline.GenerateChunk(Seed, Coord, ChunkSize, nullptr, {}, ChunkB);

	const uint32 HashA = VoxelCaveTestHelpers::HashChunk(ChunkA);
	const uint32 HashB = VoxelCaveTestHelpers::HashChunk(ChunkB);

	TestEqual(TEXT("Same seed+coordinate must produce an identical chunk hash, including carved caves"), HashA, HashB);
	return true;
}

// ---- 2. Air ratio (logged - see note on why this isn't a hard pass/fail yet) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCaveAirRatioTest, "Voxel.Generation.Cave.AirRatioLogged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelCaveAirRatioTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 32;
	FVoxelGenerationPipeline Pipeline;
	FVoxelChunk Chunk(ChunkSize);
	Pipeline.GenerateChunk(42, FVoxelChunkCoordinate(0, 0, 0), ChunkSize, nullptr, {}, Chunk);

	const float AirRatio = VoxelCaveTestHelpers::ComputeAirRatio(Chunk);
	AddInfo(FString::Printf(TEXT("Chunk (0,0,0) air ratio (including above-surface air): %.1f%%"), AirRatio * 100.0f));

	// Deliberately not a hard threshold: this chunk includes above-surface
	// air (everything from TerrainHeight up to chunk top), which dominates
	// the ratio and varies a lot by chunk Z coordinate - a tight bound here
	// would be testing chunk position, not cave carving. Once
	// SurfaceProtectionDepth/CarveThreshold are tuned against real playtesting,
	// replace this with a bound on BELOW-surface air ratio specifically
	// (would need TerrainHeight exposed per-column from the test, not just
	// the finished chunk - worth adding if this becomes a recurring need).
	TestTrue(TEXT("Air ratio should be a sane fraction, not 0% or 100%"), AirRatio > 0.0f && AirRatio < 1.0f);
	return true;
}

// ---- 3. Surface protection ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCaveSurfaceProtectionTest, "Voxel.Generation.Cave.SurfaceProtected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelCaveSurfaceProtectionTest::RunTest(const FString& Parameters)
{
	// Run passes manually (rather than via FVoxelGenerationPipeline) so the
	// test can read Context.Columns[...].TerrainHeight directly instead of
	// inferring "where the surface is" by scanning for the topmost solid
	// voxel. TerrainHeight only depends on world X/Y (see TerrainPass.cpp -
	// LocalToWorldColumn uses ChunkCoordinate.X/Y, never Z), so a cheap probe
	// pass at an arbitrary Z tells us exactly which chunk Z-layer actually
	// contains the surface for this seed, instead of guessing a Z and hoping
	// (BaseFrequency=0.01 means noise barely varies across one 32-voxel
	// chunk, so an entire chunk can legitimately sit wholly above or below
	// any single fixed Z-slice - a fixed guess was still luck-based).
	const int32 ChunkSize = 32;
	const int32 Seed = 123;

	auto RunPassesAt = [&](int32 ChunkCoordZ, FVoxelGenerationContext& OutContext, FVoxelChunk& OutChunk)
	{
		OutContext.WorldSeed = Seed;
		OutContext.ChunkCoordinate = FVoxelChunkCoordinate(0, 0, ChunkCoordZ);
		OutContext.ChunkSize = ChunkSize;
		OutContext.InitColumns();

		FClimatePass ClimatePass_;
		FBiomePass BiomePass_;
		FTerrainPass TerrainPass_;
		ClimatePass_.Execute(OutContext, OutChunk);
		BiomePass_.Execute(OutContext, OutChunk);
		TerrainPass_.Execute(OutContext, OutChunk);
	};

	// Probe at Z=0 purely to learn TerrainHeight for column (0,0) - the
	// probe chunk's own block contents are discarded.
	FVoxelGenerationContext ProbeContext;
	FVoxelChunk ProbeChunk(ChunkSize);
	RunPassesAt(0, ProbeContext, ProbeChunk);
	const int32 ProbedHeight = ProbeContext.ColumnAt(0, 0).TerrainHeight;

	const int32 TargetChunkCoordZ = FMath::FloorToInt(static_cast<float>(ProbedHeight) / ChunkSize);

	FVoxelGenerationContext Context;
	FVoxelChunk Chunk(ChunkSize);
	RunPassesAt(TargetChunkCoordZ, Context, Chunk);

	FCavePass CavePass_;
	CavePass_.Execute(Context, Chunk);

	const int32 ChunkBaseZ = TargetChunkCoordZ * ChunkSize;
	int32 ColumnsChecked = 0;
	int32 ColumnsWithProtectedSurfaceIntact = 0;

	for (int32 Y = 0; Y < ChunkSize; ++Y)
	{
		for (int32 X = 0; X < ChunkSize; ++X)
		{
			const int32 TerrainHeight = Context.ColumnAt(X, Y).TerrainHeight;
			const int32 SurfaceLocalZ = TerrainHeight - ChunkBaseZ;

			// Only meaningful if the surface actually falls inside this
			// chunk's Z range - otherwise there's nothing here to protect.
			if (SurfaceLocalZ < 0 || SurfaceLocalZ >= ChunkSize)
			{
				continue;
			}

			++ColumnsChecked;

			bool bIntact = true;
			for (int32 Depth = 0; Depth < 3; ++Depth)
			{
				const int32 LocalZ = SurfaceLocalZ - Depth;
				if (LocalZ < 0)
				{
					break; // ran off the bottom of this chunk, nothing more to check
				}
				if (Chunk.GetBlock(X, Y, LocalZ) == VoxelBlockId_Air)
				{
					bIntact = false;
					break;
				}
			}

			if (bIntact)
			{
				++ColumnsWithProtectedSurfaceIntact;
			}
		}
	}

	TestTrue(TEXT("Should have found surface columns to check (column (0,0)'s own probed height guarantees at least one)"), ColumnsChecked > 0);
	AddInfo(FString::Printf(TEXT("Surface protection intact in %d/%d columns (chunk Z=%d)"), ColumnsWithProtectedSurfaceIntact, ColumnsChecked, TargetChunkCoordZ));
	TestEqual(TEXT("Every column's near-surface voxels must remain solid (surface protection)"), ColumnsWithProtectedSurfaceIntact, ColumnsChecked);
	return true;
}

// ---- 4. Chunk boundary continuity ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCaveBoundaryContinuityTest, "Voxel.Generation.Cave.BoundaryContinuity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelCaveBoundaryContinuityTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 16;
	FVoxelGenerationPipeline Pipeline;

	FVoxelChunk ChunkA(ChunkSize); // covers world X [0, 16)
	FVoxelChunk ChunkB(ChunkSize); // covers world X [16, 32) - directly adjacent
	Pipeline.GenerateChunk(555, FVoxelChunkCoordinate(0, 0, 2), ChunkSize, nullptr, {}, ChunkA);
	Pipeline.GenerateChunk(555, FVoxelChunkCoordinate(1, 0, 2), ChunkSize, nullptr, {}, ChunkB);

	// World X=15 (ChunkA local X=15) and world X=16 (ChunkB local X=0) are
	// adjacent voxels. Because both TerrainPass and CavePass sample noise
	// in world space, solid/air state should match FAR more often across
	// this real seam than across two arbitrary unrelated columns - that's
	// the actual continuity property (not "always identical", since it's
	// still one voxel apart and noise does vary).
	int32 Matches = 0;
	int32 TotalChecked = 0;

	for (int32 Y = 0; Y < ChunkSize; ++Y)
	{
		for (int32 Z = 0; Z < ChunkSize; ++Z)
		{
			const bool bSolidA = ChunkA.GetBlock(ChunkSize - 1, Y, Z) != VoxelBlockId_Air;
			const bool bSolidB = ChunkB.GetBlock(0, Y, Z) != VoxelBlockId_Air;
			++TotalChecked;
			if (bSolidA == bSolidB)
			{
				++Matches;
			}
		}
	}

	const float MatchRatio = TotalChecked > 0 ? static_cast<float>(Matches) / TotalChecked : 0.0f;
	AddInfo(FString::Printf(TEXT("Boundary solid/air match ratio between adjacent chunks: %.1f%%"), MatchRatio * 100.0f));

	// Threshold chosen well below "always matches" (noise legitimately
	// varies voxel-to-voxel) but well above "no correlation" (~50% for
	// independent random data) - catches a regression to chunk-LOCAL noise
	// sampling, which would make this ratio collapse toward 50%.
	TestTrue(TEXT("Adjacent chunk boundary should show strong solid/air correlation, not chunk-local discontinuity"), MatchRatio > 0.75f);
	return true;
}

// ---- 5. Performance logging (informational, no hard threshold yet) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelGenerationPerfLogTest, "Voxel.Generation.PerfLog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoxelGenerationPerfLogTest::RunTest(const FString& Parameters)
{
	const int32 ChunkSize = 32;
	FVoxelGenerationPipeline Pipeline;
	FVoxelChunk Chunk(ChunkSize);

	const double StartSeconds = FPlatformTime::Seconds();
	Pipeline.GenerateChunk(2026, FVoxelChunkCoordinate(0, 0, 2), ChunkSize, nullptr, {}, Chunk);
	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

	AddInfo(FString::Printf(TEXT("Full pipeline (Climate+Biome+Terrain+Cave), %dx%dx%d chunk: %.3f ms"), ChunkSize, ChunkSize, ChunkSize, ElapsedMs));

	// No pass/fail threshold yet per the roadmap's own recommendation
	// ("not necessarily with a pass/fail threshold at first, but at least
	// log it") - this exists so generation time is visible in every test
	// run's log, and a real budget can be added once there's a profiled
	// baseline from an actual device.
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
