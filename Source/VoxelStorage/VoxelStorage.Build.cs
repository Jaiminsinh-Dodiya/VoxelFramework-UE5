using UnrealBuildTool;

public class VoxelStorage : ModuleRules
{
	public VoxelStorage(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Deliberately NOT dependent on VoxelMath - storage knows Block IDs,
		// coordinates, and compression only. No noise, no generation concepts.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"VoxelCore",
			"VoxelRuntime",
		});
	}
}
