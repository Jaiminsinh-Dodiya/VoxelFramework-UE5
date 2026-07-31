using UnrealBuildTool;

public class VoxelDebug : ModuleRules
{
	public VoxelDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VoxelCore",
			"VoxelStorage",
			"VoxelGeneration",
			"VoxelAssets",
		});
	}
}
