// VoxelRuntimeSettings.h
//
// Purpose:
//   Project Settings entry for framework-wide tunables that don't belong
//   to any single module (chunk size, world height, performance budgets).
//   Per-module settings (e.g. mesh LOD distances) belong in that module,
//   not here - this is only for values genuinely shared across modules.
//
// Responsibilities: plain data + UPROPERTY exposure. No logic.
// Thread ownership: Game Thread only (UDeveloperSettings convention).
// Dependencies: Core, CoreUObject, Engine, DeveloperSettings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoxelRuntimeSettings.generated.h"

/**
 * Project-wide voxel framework settings.
 * Edit via Project Settings > Plugins > Voxel Framework.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Voxel Framework"))
class VOXELRUNTIME_API UVoxelRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Number of voxels along one edge of a chunk. Must be a power of two. */
	UPROPERTY(EditAnywhere, Config, Category = "World", meta = (ClampMin = "8", ClampMax = "64"))
	int32 ChunkSize = 32;

	/** World height in chunks (vertical extent of the loaded voxel volume). */
	UPROPERTY(EditAnywhere, Config, Category = "World", meta = (ClampMin = "1"))
	int32 WorldHeightInChunks = 8;

	/** Chebyshev chunk radius within which chunks must have collision + finalized mesh. */
	UPROPERTY(EditAnywhere, Config, Category = "Streaming", meta = (ClampMin = "1"))
	int32 SimulationDistance = 4;

	/** Chebyshev chunk radius within which chunks are rendered. Must be >= SimulationDistance. */
	UPROPERTY(EditAnywhere, Config, Category = "Streaming", meta = (ClampMin = "1"))
	int32 RenderDistance = 8;

	/** Chebyshev chunk radius within which chunk data is generated but not necessarily meshed. */
	UPROPERTY(EditAnywhere, Config, Category = "Streaming", meta = (ClampMin = "1"))
	int32 GenerationDistance = 10;

	/** Chebyshev chunk radius within which modified chunks are kept resident for serialization. */
	UPROPERTY(EditAnywhere, Config, Category = "Streaming", meta = (ClampMin = "1"))
	int32 PersistenceDistance = 12;

	/** Target frame budget for streaming decision-making on the Game Thread, in milliseconds. See Docs/ADR.md. */
	UPROPERTY(EditAnywhere, Config, Category = "Performance", meta = (ClampMin = "0.1"))
	float StreamingBudgetMs = 1.5f;

	/** Target frame budget for rendering submission on the Game Thread, in milliseconds. */
	UPROPERTY(EditAnywhere, Config, Category = "Performance", meta = (ClampMin = "0.1"))
	float RenderSubmissionBudgetMs = 1.0f;

	/** Approximate memory budget for resident chunk storage + mesh buffers, in megabytes. */
	UPROPERTY(EditAnywhere, Config, Category = "Performance", meta = (ClampMin = "16"))
	int32 MemoryBudgetMB = 256;
};
