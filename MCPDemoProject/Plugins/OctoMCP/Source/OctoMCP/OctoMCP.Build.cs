using UnrealBuildTool;

public class OctoMCP : ModuleRules
{
	public OctoMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTPServer",
				"Json",
				"Projects"
			});
	}
}
