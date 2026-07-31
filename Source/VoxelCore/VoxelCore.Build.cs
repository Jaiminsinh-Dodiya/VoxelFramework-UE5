using UnrealBuildTool;

public class VoxelCore : ModuleRules
{
	public VoxelCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// VoxelCore is a leaf module: types, interfaces, enums, delegates, logging.
		// No module startup logic, no owned systems -> Engine dependency only,
		// not Projects (that belongs to VoxelRuntime, which does module lifecycle work).
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
		});
	}
}
