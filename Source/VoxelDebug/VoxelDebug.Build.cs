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
			"VoxelMeshing",              // new: real greedy-meshed preview mode
			"ProceduralMeshComponent",   // debug-tool-only exception to ADR-004 - see VoxelDebugVisualizer.h
		});
	}
}
