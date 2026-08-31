using UnrealBuildTool;

public class VoxelStreaming : ModuleRules
{
	public VoxelStreaming(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VoxelCore",
			"VoxelRuntime",
			"VoxelStorage",
			"VoxelWorld",
		});
	}
}
