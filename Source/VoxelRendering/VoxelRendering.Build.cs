using UnrealBuildTool;

public class VoxelRendering : ModuleRules
{
	public VoxelRendering(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VoxelCore",
			"VoxelMeshing", // consumes FVoxelMeshData - this module has ZERO knowledge of chunks, generation, or storage
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore", // FLocalVertexFactory, FStaticMeshVertexBuffers, FPositionVertexBuffer, FColorVertexBuffer
			"RHI",        // RHI resource creation/update
		});
	}
}
