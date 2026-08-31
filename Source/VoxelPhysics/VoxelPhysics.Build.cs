using UnrealBuildTool;

public class VoxelPhysics : ModuleRules
{
	public VoxelPhysics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"VoxelCore",
			"VoxelRuntime",
			"VoxelAssets",
			"VoxelStorage",
			"VoxelMeshing",
		});
	}
}
