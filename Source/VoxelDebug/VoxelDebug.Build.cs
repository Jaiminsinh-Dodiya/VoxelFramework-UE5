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
			"VoxelWorld",                // subsystem integration test (RequestChunksViaSubsystem)
			"VoxelStreaming",            // live diagnostics stats (UVoxelStreamingManager)
			"VoxelRuntime",              // transitive dependency of VoxelWorld
			"ProceduralMeshComponent",   // debug-tool-only exception to ADR-004 - see VoxelDebugVisualizer.h
		});
	}
}
