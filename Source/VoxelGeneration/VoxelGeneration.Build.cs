using UnrealBuildTool;

public class VoxelGeneration : ModuleRules
{
	public VoxelGeneration(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"VoxelCore",
			"VoxelMath",     // noise - generation is the only module allowed to depend on this
			"VoxelAssets",   // block/biome definitions
			"VoxelStorage",  // writes into FVoxelChunk
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"VoxelRuntime", // FVoxelScheduler, once chunk generation is dispatched async in Phase 5
		});
	}
}
