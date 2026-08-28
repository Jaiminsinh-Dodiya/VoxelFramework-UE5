using UnrealBuildTool;

public class VoxelWorld : ModuleRules
{
	public VoxelWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VoxelCore",
			"VoxelRuntime",
			"VoxelAssets",
			"VoxelStorage",
			"VoxelGeneration",
			"VoxelMeshing",
			"VoxelRendering",
			"DeveloperSettings", // UVoxelWorldSettings
		});
	}
}
