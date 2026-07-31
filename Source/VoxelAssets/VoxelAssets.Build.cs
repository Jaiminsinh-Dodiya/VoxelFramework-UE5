using UnrealBuildTool;

public class VoxelAssets : ModuleRules
{
	public VoxelAssets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine", // UDataAsset, UMaterialInterface
			"GameplayTags", // FGameplayTagContainer on biome definitions
			"VoxelCore",
		});
	}
}
