// VoxelMeshingService.cpp

#include "VoxelMeshingService.h"
#include "VoxelMesher.h"
#include "VoxelMeshData.h"
#include "VoxelRuntimeModule.h"
#include "VoxelScheduler.h"

namespace VoxelMeshingService
{
	FVoxelJobHandle RequestMeshAsync(
		const FVoxelChunk* Chunk,
		const UVoxelBlockRegistry* BlockRegistry,
		TFunction<void(FVoxelMeshData&&)> OnComplete)
	{
		check(Chunk); // see header lifetime caveat - caller must guarantee this outlives the job

		// Result is produced inside Work and handed to OnComplete via a
		// shared mutable slot, since FVoxelScheduler::Submit's Work and
		// OnComplete are two separate callbacks rather than one that
		// returns a value.
		TSharedRef<FVoxelMeshData> Result = MakeShared<FVoxelMeshData>();

		return FVoxelRuntimeModule::Get().GetScheduler().Submit(
			[Chunk, BlockRegistry, Result]()
			{
				*Result = FVoxelMesher::GenerateMesh(*Chunk, BlockRegistry);
			},
			EVoxelWorkPriority::Normal,
			[Result, OnComplete]()
			{
				if (OnComplete)
				{
					OnComplete(MoveTemp(*Result));
				}
			});
	}
}
