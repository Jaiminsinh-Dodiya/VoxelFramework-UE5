// VoxelMesher.cpp
//
// Implementation notes (read before modifying):
//
// Algorithm is the standard binary greedy meshing sweep (as popularized by
// Mikola Lysenko's "Meshing in a Minecraft Game" writeup): for each of the
// 3 axes and both directions along that axis, sweep slices perpendicular to
// the axis, build a 2D mask of "is there a visible face here, and if so
// which material/orientation", then greedily grow rectangles across
// matching mask cells.
//
// Vertex deduplication: NOT implemented. Each emitted quad gets its own 4
// vertices, even where quads share an edge. This is simpler and correct,
// just not maximally memory-efficient - shared-vertex welding is a
// documented future optimization (see Docs/TODO.md), not attempted here to
// keep this first pass's correctness easier to verify and test.
//
// Ambient occlusion: computed PER VERTEX, independent of how large the
// merged quad is (i.e. NOT part of the merge key - geometry merges purely
// by material, and AO is evaluated afterward per corner position). This is
// the standard approach and avoids the alternative (baking AO into the
// merge key, which would prevent most real terrain from merging at all
// since neighbor counts vary voxel-to-voxel). The corner AO formula is the
// well-known "0fps.net" scheme: for a corner touching two edge-neighbor
// cells and one diagonal-neighbor cell (relative to that corner, in the
// plane of the face, at the depth of the solid voxel), occlusion strength
// is 3 - (side1 + side2 + corner), with a hard 0 (fully occluded) special
// case when both edge neighbors are solid (matches how light actually
// can't reach a corner boxed in on both sides even if the diagonal is open).
//
// Honesty note: this was implemented and reasoned through carefully but has
// NOT been validated against a reference-image/known-good AO renderer -
// automation tests below check variance and directional correctness (more
// occluded corners are darker), not pixel-exact values. Treat visual
// results as "first pass, worth eyeballing once VoxelRendering exists" per
// the project's own "visual validation gate" before assuming it's final.

#include "VoxelMesher.h"
#include "VoxelChunk.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBlockDefinition.h"

namespace
{
	struct FMaskCell
	{
		FVoxelBlockId BlockId = VoxelBlockId_Air;
		bool bBackFace = false;

		bool MatchesForMerge(const FMaskCell& Other) const
		{
			return BlockId == Other.BlockId && bBackFace == Other.bBackFace;
		}
	};

	int32 ResolveMaterialId(FVoxelBlockId BlockId, const UVoxelBlockRegistry* BlockRegistry)
	{
		if (BlockRegistry)
		{
			if (const UVoxelBlockDefinition* Definition = BlockRegistry->FindDefinition(BlockId))
			{
				return Definition->MaterialLayerIndex;
			}
		}
		return static_cast<int32>(BlockId); // fallback: group by raw block ID
	}

	FLinearColor ResolveVertexTint(FVoxelBlockId BlockId, const UVoxelBlockRegistry* BlockRegistry)
	{
		if (BlockRegistry)
		{
			if (const UVoxelBlockDefinition* Definition = BlockRegistry->FindDefinition(BlockId))
			{
				return Definition->VertexTint;
			}
		}
		return FLinearColor::White;
	}

	/** 0fps.net-style corner AO: 0 = fully occluded, 3 = fully open. */
	int32 CornerAO(bool bSide1, bool bSide2, bool bCorner)
	{
		if (bSide1 && bSide2)
		{
			return 0;
		}
		return 3 - (static_cast<int32>(bSide1) + static_cast<int32>(bSide2) + static_cast<int32>(bCorner));
	}
}

FVoxelMeshData FVoxelMesher::GenerateMesh(const FVoxelChunk& Chunk, const UVoxelBlockRegistry* BlockRegistry)
{
	FVoxelMeshData MeshData;

	const int32 Size = Chunk.GetSize();
	if (Size <= 0)
	{
		return MeshData;
	}

	// Chunk-edge voxels face air beyond the boundary - see header comment
	// on cross-chunk stitching being explicitly out of scope here.
	auto GetBlock = [&](int32 X, int32 Y, int32 Z) -> FVoxelBlockId
	{
		if (X < 0 || Y < 0 || Z < 0 || X >= Size || Y >= Size || Z >= Size)
		{
			return VoxelBlockId_Air;
		}
		return Chunk.GetBlock(X, Y, Z);
	};

	// Map from MaterialId -> index into MeshData.Sections, so repeated
	// materials across different sweep axes/slices share one section.
	TMap<int32, int32> MaterialIdToSectionIndex;
	auto GetOrAddSection = [&](int32 MaterialId) -> FVoxelMeshSection&
	{
		if (const int32* Existing = MaterialIdToSectionIndex.Find(MaterialId))
		{
			return MeshData.Sections[*Existing];
		}
		const int32 NewIndex = MeshData.Sections.Add(FVoxelMeshSection{ MaterialId, {} });
		MaterialIdToSectionIndex.Add(MaterialId, NewIndex);
		return MeshData.Sections[NewIndex];
	};

	for (int32 D = 0; D < 3; ++D)
	{
		const int32 U = (D + 1) % 3;
		const int32 V = (D + 2) % 3;

		int32 X[3] = { 0, 0, 0 };
		int32 Q[3] = { 0, 0, 0 };
		Q[D] = 1;

		TArray<FMaskCell> Mask;
		Mask.SetNum(Size * Size);

		// Slices run from -1 to Size-1 inclusive: X[D]=-1 means "the face
		// between nothing and voxel 0", X[D]=Size-1 means "the face between
		// voxel Size-1 and nothing" - Size+1 total slice boundaries for Size voxels.
		for (X[D] = -1; X[D] < Size; ++X[D])
		{
			// --- Build mask for this slice ---
			int32 N = 0;
			for (X[V] = 0; X[V] < Size; ++X[V])
			{
				for (X[U] = 0; X[U] < Size; ++X[U])
				{
					const FVoxelBlockId BlockA = (X[D] >= 0) ? GetBlock(X[0], X[1], X[2]) : VoxelBlockId_Air;
					const FVoxelBlockId BlockB = (X[D] < Size - 1)
						? GetBlock(X[0] + Q[0], X[1] + Q[1], X[2] + Q[2])
						: VoxelBlockId_Air;

					const bool bSolidA = BlockA != VoxelBlockId_Air;
					const bool bSolidB = BlockB != VoxelBlockId_Air;

					if (bSolidA == bSolidB)
					{
						Mask[N++] = FMaskCell{}; // both solid (hidden face) or both air (nothing here) - no face
					}
					else
					{
						// bBackFace=true means B is the solid side (face normal points toward -D).
						Mask[N++] = FMaskCell{ bSolidA ? BlockA : BlockB, !bSolidA };
					}
				}
			}

			// --- Greedy-merge the mask into quads ---
			N = 0;
			for (int32 J = 0; J < Size; ++J)
			{
				for (int32 I = 0; I < Size; )
				{
					const FMaskCell Cell = Mask[N];
					if (Cell.BlockId == VoxelBlockId_Air)
					{
						++I;
						++N;
						continue;
					}

					// Grow width along U.
					int32 Width = 1;
					while (I + Width < Size && Mask[N + Width].MatchesForMerge(Cell))
					{
						++Width;
					}

					// Grow height along V, requiring the whole row to match.
					int32 Height = 1;
					bool bDone = false;
					while (J + Height < Size)
					{
						for (int32 K = 0; K < Width; ++K)
						{
							if (!Mask[N + K + Height * Size].MatchesForMerge(Cell))
							{
								bDone = true;
								break;
							}
						}
						if (bDone)
						{
							break;
						}
						++Height;
					}

					// --- Emit the quad ---
					// Solid-side depth coordinate, used for AO neighbor lookups -
					// the solid voxel is at X[D] if A was solid (bBackFace=false),
					// or X[D]+1 if B was solid (bBackFace=true).
					const int32 SolidDepth = Cell.bBackFace ? X[D] + 1 : X[D];

					const int32 QuadMinU = I;
					const int32 QuadMinV = J;
					const int32 QuadMaxU = I + Width;
					const int32 QuadMaxV = J + Height;

					auto SolidAt = [&](int32 CellU, int32 CellV) -> bool
					{
						int32 P[3] = { 0, 0, 0 };
						P[D] = SolidDepth;
						P[U] = CellU;
						P[V] = CellV;
						return GetBlock(P[0], P[1], P[2]) != VoxelBlockId_Air;
					};

					// Corner AO evaluated per actual lattice corner, independent of merge size.
					auto ComputeCornerColor = [&](int32 CornerU, int32 CornerV) -> float
					{
						const int32 OutsideU = (CornerU == QuadMinU) ? CornerU - 1 : CornerU;
						const int32 OutsideV = (CornerV == QuadMinV) ? CornerV - 1 : CornerV;
						const int32 InsideVForU = (CornerV == QuadMinV) ? CornerV : CornerV - 1;
						const int32 InsideUForV = (CornerU == QuadMinU) ? CornerU : CornerU - 1;

						const bool bSide1 = SolidAt(OutsideU, InsideVForU);
						const bool bSide2 = SolidAt(InsideUForV, OutsideV);
						const bool bCorner = SolidAt(OutsideU, OutsideV);

						const int32 AO = CornerAO(bSide1, bSide2, bCorner);
						return 0.25f + 0.75f * (static_cast<float>(AO) / 3.0f); // avoid fully-black corners
					};

					FVector Base(0.0f);
					Base[D] = static_cast<float>(X[D] + 1); // face plane sits at the boundary just generated
					Base[U] = static_cast<float>(I);
					Base[V] = static_cast<float>(J);

					FVector DU(0.0f);
					DU[U] = static_cast<float>(Width);
					FVector DV(0.0f);
					DV[V] = static_cast<float>(Height);

					const int32 MaterialId = ResolveMaterialId(Cell.BlockId, BlockRegistry);
					const FLinearColor Tint = ResolveVertexTint(Cell.BlockId, BlockRegistry);

					FVector Normal(0.0f);
					Normal[D] = Cell.bBackFace ? -1.0f : 1.0f;

					const float AO00 = ComputeCornerColor(QuadMinU, QuadMinV);
					const float AO10 = ComputeCornerColor(QuadMaxU, QuadMinV);
					const float AO11 = ComputeCornerColor(QuadMaxU, QuadMaxV);
					const float AO01 = ComputeCornerColor(QuadMinU, QuadMaxV);

					const int32 BaseVertexIndex = MeshData.Vertices.Num();

					FVoxelMeshVertex V00, V10, V11, V01;
					V00.Position = Base;                V00.Normal = Normal; V00.UV = FVector2D(0, 0);           V00.Color = Tint * AO00;
					V10.Position = Base + DU;            V10.Normal = Normal; V10.UV = FVector2D(Width, 0);       V10.Color = Tint * AO10;
					V11.Position = Base + DU + DV;       V11.Normal = Normal; V11.UV = FVector2D(Width, Height);  V11.Color = Tint * AO11;
					V01.Position = Base + DV;            V01.Normal = Normal; V01.UV = FVector2D(0, Height);      V01.Color = Tint * AO01;

					MeshData.Vertices.Add(V00);
					MeshData.Vertices.Add(V10);
					MeshData.Vertices.Add(V11);
					MeshData.Vertices.Add(V01);

					FVoxelMeshSection& Section = GetOrAddSection(MaterialId);
					if (!Cell.bBackFace)
					{
						// Front face (+D normal): CCW winding as seen from +D.
						Section.Indices.Append({
							static_cast<uint32>(BaseVertexIndex + 0), static_cast<uint32>(BaseVertexIndex + 1), static_cast<uint32>(BaseVertexIndex + 2),
							static_cast<uint32>(BaseVertexIndex + 0), static_cast<uint32>(BaseVertexIndex + 2), static_cast<uint32>(BaseVertexIndex + 3) });
					}
					else
					{
						// Back face (-D normal): reversed winding.
						Section.Indices.Append({
							static_cast<uint32>(BaseVertexIndex + 0), static_cast<uint32>(BaseVertexIndex + 3), static_cast<uint32>(BaseVertexIndex + 2),
							static_cast<uint32>(BaseVertexIndex + 0), static_cast<uint32>(BaseVertexIndex + 2), static_cast<uint32>(BaseVertexIndex + 1) });
					}

					// Zero out the consumed mask region.
					for (int32 L = 0; L < Height; ++L)
					{
						for (int32 K = 0; K < Width; ++K)
						{
							Mask[N + K + L * Size] = FMaskCell{};
						}
					}

					I += Width;
					N += Width;
				}
			}
		}
	}

	return MeshData;
}
