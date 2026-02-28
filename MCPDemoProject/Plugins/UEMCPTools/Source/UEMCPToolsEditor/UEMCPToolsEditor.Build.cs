using UnrealBuildTool;

public class UEMCPToolsEditor : ModuleRules
{
	public UEMCPToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EditorSubsystem",
				"UnrealEd"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"AssetTools",
				"BlueprintGraph",
				"DeveloperSettings",
				"HTTPServer",
				"Json",
				"JsonUtilities",
				"Kismet",
				"Slate",
				"SlateCore",
				"UMG",
				"UMGEditor",
				"UnrealEd"
			}
		);
	}
}
