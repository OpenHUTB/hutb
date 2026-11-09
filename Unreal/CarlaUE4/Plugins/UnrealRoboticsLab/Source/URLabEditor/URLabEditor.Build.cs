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
			"LevelEditor",
			"EditorStyle"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"RenderCore",
			"DesktopPlatform",
			"Kismet"
		});

		// UE4.26 的 Blutility 模块把 BlutilityMenuExtensions.h 放在 Private/ 下，
		// 需要手动补充私有 include 路径。
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Editor", "Blutility", "Private"));

		// ROV 桥接测试需要引用 AirSim 插件的头文件（Vehicles/Rov/*.h）。
		// 依赖 AirSim 模块只保证链接，头文件路径需显式声明。
		PrivateDependencyModuleNames.Add("AirSim");
		PrivateIncludePaths.Add(Path.Combine(PluginDirectory, "..", "AirSim", "Source"));
	}
}
