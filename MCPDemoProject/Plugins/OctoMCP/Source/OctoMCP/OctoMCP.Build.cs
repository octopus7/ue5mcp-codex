using UnrealBuildTool;

public class OctoMCP : ModuleRules
{
	public OctoMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetTools",
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"HTTPServer",
				"Json",
				"Projects",
				"UMG",
				"UMGEditor",
				"UnrealEd"
			});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("LiveCoding");
		}
	}
}
