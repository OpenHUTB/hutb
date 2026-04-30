using UnrealBuildTool;
using System.IO;

public class URLabEditor : ModuleRules
{
	public URLabEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"URLab",
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"Blutility",
			"EditorScriptingUtilities",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"XmlParser",
			"Projects",
			"LevelEditor"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"RenderCore",
			"DesktopPlatform",
			"Kismet"
		});
	}
}
