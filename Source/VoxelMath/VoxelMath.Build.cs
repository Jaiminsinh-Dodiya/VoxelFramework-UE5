using UnrealBuildTool;

public class VoxelMath : ModuleRules
{
	public VoxelMath(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Deliberately does NOT depend on VoxelStorage (ADR: "Storage should
		// not depend on Math" - the inverse is fine, Math is a pure-function
		// leaf used BY Generation, never the other way around).
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"VoxelCore",
		});
	}
}
