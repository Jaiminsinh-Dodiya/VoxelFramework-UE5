using UnrealBuildTool;

public class VoxelRuntime : ModuleRules
{
	public VoxelRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VoxelCore",
			"DeveloperSettings", // for UVoxelRuntimeSettings
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects", // module lifecycle only
		});
	}
}
