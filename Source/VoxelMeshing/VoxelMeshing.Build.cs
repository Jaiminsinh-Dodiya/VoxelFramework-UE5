using UnrealBuildTool;

public class VoxelMeshing : ModuleRules
{
	public VoxelMeshing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Deliberately NO Engine rendering modules (RenderCore, RHI, etc.) and
		// no dependency that would pull in UMeshComponent/FPrimitiveSceneProxy -
		// this module produces plain CPU-side arrays only, per ADR-004.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"VoxelCore",
			"VoxelStorage",
			"VoxelAssets", // optional: block registry lookup for material index / vertex tint
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"VoxelRuntime", // FVoxelScheduler for async dispatch helper - no new scheduling abstraction
			"VoxelGeneration", // test-only: build real terrain chunks for determinism/perf tests
		});
	}
}
