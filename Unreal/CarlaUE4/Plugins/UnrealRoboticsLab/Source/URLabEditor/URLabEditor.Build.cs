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

		// OpenHUTB 追加（2026-11-04）：
		// UE4.26 的 Blutility 模块把 BlutilityMenuExtensions.h 放在 Private/ 下，
		// 而 AssetActionUtility.h（MujocoGenerationAction.h 依赖它）会 include 该头文件。
		// 仅声明 Blutility 依赖不够，必须手动补这条私有 include 路径，
		// 否则报 C1083: 无法打开包含文件 "BlutilityMenuExtensions.h"。
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Editor", "Blutility", "Private"));
	}
}
