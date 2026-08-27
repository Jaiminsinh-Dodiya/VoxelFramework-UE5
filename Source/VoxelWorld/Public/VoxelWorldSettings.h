// VoxelWorldSettings.h
//
// Purpose:
//   Project-wide defaults for UVoxelWorldSubsystem: world seed, default
//   biome set, and rendering material assignment. Intentionally NOT the
//   final "World Definition" asset the design checkpoint (Docs/ARCHITECTURE.md
//   #8) calls for eventually - that needs per-world-instance parameters
//   (island size, sea level, coastline shape) that don't belong in
//   compiled project config. This exists so VoxelWorldSubsystem has
//   somewhere to read a seed/biome list from RIGHT NOW, without blocking
//   on the Region/Island design decisions. Replace this with a real World
//   Definition data asset when that design lands - don't build on top of
//   it as if it were permanent.
//
// Responsibilities: plain config data, no logic.
// Thread ownership: Game Thread only (UDeveloperSettings convention).
// Dependencies: Core, CoreUObject, Engine, VoxelAssets, DeveloperSettings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoxelWorldSettings.generated.h"

class UVoxelBiomeDefinition;
class UMaterialInterface;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Voxel World (Temporary Defaults)"))
class VOXELWORLD_API UVoxelWorldSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Passed to FVoxelGenerationPipeline for every chunk this subsystem requests. */
	UPROPERTY(EditAnywhere, Config, Category = "World")
	int32 WorldSeed = 1234;

	/** Passed as AvailableBiomes to generation. Empty = TerrainPass's flat fallback layering. Resolved and precached once at subsystem Initialize. */
	UPROPERTY(EditAnywhere, Config, Category = "World")
	TArray<TSoftObjectPtr<UVoxelBiomeDefinition>> DefaultBiomes;

	/** World-space size of one voxel, in Unreal units. Baked into mesh vertex positions when a chunk's mesh is created. */
	UPROPERTY(EditAnywhere, Config, Category = "Rendering", meta = (ClampMin = "1"))
	float VoxelWorldSize = 100.0f;

	/** Per-material-ID material override for rendered chunks. Same convention as VoxelDebug's BlockMaterials - keyed by resolved MaterialId (raw FVoxelBlockId when no block registry material mapping is used). */
	UPROPERTY(EditAnywhere, Config, Category = "Rendering")
	TMap<int32, TSoftObjectPtr<UMaterialInterface>> BlockMaterials;

	/** Used for any material ID not present in BlockMaterials. */
	UPROPERTY(EditAnywhere, Config, Category = "Rendering")
	TSoftObjectPtr<UMaterialInterface> DefaultMaterial;
};
