// VoxelCollisionBuilder.cpp

#include "VoxelCollisionBuilder.h"
#include "VoxelChunk.h"
#include "VoxelBlockRegistry.h"
#include "VoxelBlockDefinition.h"
#include "VoxelMesher.h"

namespace
{
	FORCEINLINE bool IsVoxelCollidable(FVoxelBlockId BlockId, const UVoxelBlockRegistry* Registry)
	{
		if (BlockId == VoxelBlockId_Air)
		{
			return false;
		}
		if (Registry)
		{
			if (const UVoxelBlockDefinition* Def = Registry->FindDefinition(BlockId))
			{
				return Def->bIsSolid && Def->bGeneratesCollision;
			}
		}
		return true;
	}

	FORCEINLINE bool GetNeighborCollidable(
		int32 X, int32 Y, int32 Z,
		int32 Size,
		const FVoxelChunk& Chunk,
		const UVoxelBlockRegistry* Registry,
		const FVoxelNeighborChunks* Neighbors)
	{
		if (X >= 0 && X < Size && Y >= 0 && Y < Size && Z >= 0 && Z < Size)
		{
			return IsVoxelCollidable(Chunk.GetBlock(X, Y, Z), Registry);
		}

		if (!Neighbors)
		{
			return false; // Air fallback
		}

		if (X < 0)
		{
			return Neighbors->NegX ? IsVoxelCollidable(Neighbors->NegX->GetBlock(Size - 1, Y, Z), Registry) : false;
		}
		if (X >= Size)
		{
			return Neighbors->PosX ? IsVoxelCollidable(Neighbors->PosX->GetBlock(0, Y, Z), Registry) : false;
		}
		if (Y < 0)
		{
			return Neighbors->NegY ? IsVoxelCollidable(Neighbors->NegY->GetBlock(X, Size - 1, Z), Registry) : false;
		}
		if (Y >= Size)
		{
			return Neighbors->PosY ? IsVoxelCollidable(Neighbors->PosY->GetBlock(X, 0, Z), Registry) : false;
		}
		if (Z < 0)
		{
			return Neighbors->NegZ ? IsVoxelCollidable(Neighbors->NegZ->GetBlock(X, Y, Size - 1), Registry) : false;
		}
		if (Z >= Size)
		{
			return Neighbors->PosZ ? IsVoxelCollidable(Neighbors->PosZ->GetBlock(X, Y, 0), Registry) : false;
		}

		return false;
	}
}

FVoxelCollisionData FVoxelCollisionBuilder::BuildCollisionData(
	const FVoxelChunk& Chunk,
	const UVoxelBlockRegistry* BlockRegistry,
	const FVoxelNeighborChunks* Neighbors,
	const FVoxelChunkCoordinate* Coordinate,
	float VoxelWorldSize,
	uint32 CollisionRevision,
	EVoxelCollisionMode Mode)
{
	FVoxelCollisionData Result;
	Result.Coordinate = Coordinate ? *Coordinate : FVoxelChunkCoordinate();
	Result.CollisionRevision = CollisionRevision;

	if (Mode == EVoxelCollisionMode::None || Chunk.IsEmpty())
	{
		Result.bIsEmpty = true;
		return Result;
	}

	const int32 Size = Chunk.GetSize();
	const FVector3f WorldOffset = Coordinate
		? FVector3f(
			static_cast<float>(Coordinate->X * Size) * VoxelWorldSize,
			static_cast<float>(Coordinate->Y * Size) * VoxelWorldSize,
			static_cast<float>(Coordinate->Z * Size) * VoxelWorldSize)
		: FVector3f::ZeroVector;

	// Mask for greedy quad merging per slice: values are +1 (pos face), -1 (neg face), 0 (no face)
	TArray<int8> Mask;
	Mask.SetNumUninitialized(Size * Size);

	TArray<FVector3f>& Vertices = Result.Vertices;
	TArray<FTriIndices>& Indices = Result.Indices;

	// Reserve capacity dynamically scaled with chunk volume (avoids repeated reallocations)
	const int32 EstimatedQuads = FMath::Max(128, (Size * Size * Size) / 16);
	Vertices.Reserve(EstimatedQuads * 4);
	Indices.Reserve(EstimatedQuads * 2);

	// Sweep along 3 primary dimensions (0: X-axis, 1: Y-axis, 2: Z-axis)
	for (int32 Dimension = 0; Dimension < 3; ++Dimension)
	{
		const int32 U = (Dimension + 1) % 3;
		const int32 V = (Dimension + 2) % 3;

		int32 Pos[3] = { 0, 0, 0 };
		int32 Q[3] = { 0, 0, 0 };
		Q[Dimension] = 1;

		for (Pos[Dimension] = -1; Pos[Dimension] < Size; ++Pos[Dimension])
		{
			// 1. Build Face Mask between Pos[Dimension] and Pos[Dimension] + 1
			int32 MaskIndex = 0;
			for (Pos[V] = 0; Pos[V] < Size; ++Pos[V])
			{
				for (Pos[U] = 0; Pos[U] < Size; ++Pos[U])
				{
					const bool bBlockCurrent = GetNeighborCollidable(Pos[0], Pos[1], Pos[2], Size, Chunk, BlockRegistry, Neighbors);
					const bool bBlockNext = GetNeighborCollidable(Pos[0] + Q[0], Pos[1] + Q[1], Pos[2] + Q[2], Size, Chunk, BlockRegistry, Neighbors);

					if (bBlockCurrent != bBlockNext)
					{
						Mask[MaskIndex] = bBlockCurrent ? 1 : -1;
					}
					else
					{
						Mask[MaskIndex] = 0;
					}
					++MaskIndex;
				}
			}

			// 2. Greedily Merge Faces in Mask
			MaskIndex = 0;
			for (int32 J = 0; J < Size; ++J)
			{
				for (int32 I = 0; I < Size;)
				{
					const int8 NormalDir = Mask[MaskIndex];
					if (NormalDir != 0)
					{
						// Compute width of merged quad
						int32 Width = 1;
						while (I + Width < Size && Mask[MaskIndex + Width] == NormalDir)
						{
							++Width;
						}

						// Compute height of merged quad
						int32 Height = 1;
						bool bCanExtend = true;
						while (J + Height < Size && bCanExtend)
						{
							for (int32 K = 0; K < Width; ++K)
							{
								if (Mask[MaskIndex + K + Height * Size] != NormalDir)
								{
									bCanExtend = false;
									break;
								}
							}
							if (bCanExtend)
							{
								++Height;
							}
						}

						// Emit Quad Vertices
						Pos[U] = I;
						Pos[V] = J;

						int32 Du[3] = { 0, 0, 0 };
						int32 Dv[3] = { 0, 0, 0 };
						Du[U] = Width;
						Dv[V] = Height;

						const int32 BaseVertexIndex = Vertices.Num();

						FVector3f V0(static_cast<float>(Pos[0] + Q[0]), static_cast<float>(Pos[1] + Q[1]), static_cast<float>(Pos[2] + Q[2]));
						FVector3f V1(static_cast<float>(Pos[0] + Q[0] + Du[0]), static_cast<float>(Pos[1] + Q[1] + Du[1]), static_cast<float>(Pos[2] + Q[2] + Du[2]));
						FVector3f V2(static_cast<float>(Pos[0] + Q[0] + Du[0] + Dv[0]), static_cast<float>(Pos[1] + Q[1] + Du[1] + Dv[1]), static_cast<float>(Pos[2] + Q[2] + Du[2] + Dv[2]));
						FVector3f V3(static_cast<float>(Pos[0] + Q[0] + Dv[0]), static_cast<float>(Pos[1] + Q[1] + Dv[1]), static_cast<float>(Pos[2] + Q[2] + Dv[2]));

						V0 = (V0 * VoxelWorldSize) + WorldOffset;
						V1 = (V1 * VoxelWorldSize) + WorldOffset;
						V2 = (V2 * VoxelWorldSize) + WorldOffset;
						V3 = (V3 * VoxelWorldSize) + WorldOffset;

						Vertices.Add(V0);
						Vertices.Add(V1);
						Vertices.Add(V2);
						Vertices.Add(V3);

						// Update analytical bounds
						Result.Bounds += FVector(V0);
						Result.Bounds += FVector(V1);
						Result.Bounds += FVector(V2);
						Result.Bounds += FVector(V3);

						// Emit 2 Triangles based on normal direction
						FTriIndices Tri1, Tri2;
						if (NormalDir > 0)
						{
							Tri1.v0 = BaseVertexIndex + 0;
							Tri1.v1 = BaseVertexIndex + 1;
							Tri1.v2 = BaseVertexIndex + 2;

							Tri2.v0 = BaseVertexIndex + 0;
							Tri2.v1 = BaseVertexIndex + 2;
							Tri2.v2 = BaseVertexIndex + 3;
						}
						else
						{
							Tri1.v0 = BaseVertexIndex + 0;
							Tri1.v1 = BaseVertexIndex + 2;
							Tri1.v2 = BaseVertexIndex + 1;

							Tri2.v0 = BaseVertexIndex + 0;
							Tri2.v1 = BaseVertexIndex + 3;
							Tri2.v2 = BaseVertexIndex + 2;
						}

						Indices.Add(Tri1);
						Indices.Add(Tri2);

						// Clear merged region in mask
						for (int32 Row = 0; Row < Height; ++Row)
						{
							for (int32 Col = 0; Col < Width; ++Col)
							{
								Mask[MaskIndex + Col + Row * Size] = 0;
							}
						}

						I += Width;
						MaskIndex += Width;
					}
					else
					{
						++I;
						++MaskIndex;
					}
				}
			}
		}
	}

	Result.bIsEmpty = Indices.IsEmpty();
	return Result;
}
