// VoxelBlockRegistry.h
//
// Purpose:
//   Fast runtime lookup from FVoxelBlockId -> UVoxelBlockDefinition*,
//   built once at world load from whatever set of block definitions the
//   project configures. This is the ONLY place Storage/Meshing should ask
//   "what does this ID mean" - they never touch UDataAsset soft pointers
//   directly on a hot path.
//
// Responsibilities:
//   - Load configured block definitions, validate no duplicate IDs
//   - O(1) lookup by ID (flat array indexed by ID, not a TMap, since IDs
//     are dense small integers - cache-friendly per VoxelStorage's needs)
//
// Thread ownership: built on Game Thread during world init; read-only
//   access after that is safe from any thread (no mutation post-build).
// Dependencies: Core, CoreUObject, Engine, VoxelCore, VoxelBlockDefinition.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VoxelCoreTypes.h"
#include "VoxelBlockDefinition.h"
#include "VoxelBlockRegistry.generated.h"

class UVoxelBiomeDefinition;

UCLASS()
class VOXELASSETS_API UVoxelBlockRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Populates the registry from a list of block definitions (typically read from project settings/config). */
	void BuildFromDefinitions(const TArray<UVoxelBlockDefinition*>& Definitions);

	/** Returns nullptr for VoxelBlockId_Air or any unregistered ID - callers must handle both explicitly. */
	const UVoxelBlockDefinition* FindDefinition(FVoxelBlockId BlockId) const;

	bool IsSolid(FVoxelBlockId BlockId) const;

	/**
	 * Resolves every biome's TerrainLayers soft pointers to concrete
	 * FVoxelBlockId values and caches the result, keyed by biome. MUST be
	 * called from the Game Thread (LoadSynchronous on a soft pointer is not
	 * safe off the Game Thread) before any generation job that references
	 * these biomes is dispatched. Safe to call again if the biome list
	 * changes - overwrites existing cache entries.
	 */
	void PrecacheBiomeLayers(const TArray<UVoxelBiomeDefinition*>& Biomes);

	/**
	 * Read-only lookup, safe to call from any thread (including generation
	 * worker threads) as long as PrecacheBiomeLayers already ran for this
	 * biome on the Game Thread first. Returns nullptr if Biome was never
	 * precached - callers must fall back to default layering in that case.
	 */
	const TArray<FVoxelBlockId>* GetResolvedLayerBlockIds(const UVoxelBiomeDefinition* Biome) const;

private:
	// Indexed directly by BlockId. Index 0 (air) is always nullptr.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVoxelBlockDefinition>> DefinitionsById;

	// Built once on the Game Thread by PrecacheBiomeLayers, read-only after
	// that from any thread - see method comments above for the contract.
	TMap<const UVoxelBiomeDefinition*, TArray<FVoxelBlockId>> ResolvedBiomeLayers;
};
