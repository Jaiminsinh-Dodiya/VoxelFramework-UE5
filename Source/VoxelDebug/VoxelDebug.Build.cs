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
			"VoxelMeshing",              // real greedy-meshed preview mode (PMC)
			"VoxelRendering",            // real renderer preview mode (UVoxelMeshComponent)
			"ProceduralMeshComponent",   // debug-tool-only exception to ADR-004 - see VoxelDebugVisualizer.h
		});
	}
}
