// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;
using System;
using System.IO;

public class CarlaUE4Target : TargetRules
{
	public CarlaUE4Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		BuildEnvironment = TargetBuildEnvironment.Shared;
		bOverrideBuildEnvironment = true;
		ExtraModuleNames.Add("CarlaUE4");

    string ConfigDir = Path.GetDirectoryName(ProjectFile.ToString()) + "/Config/";
    string OptionalModulesFile = Path.Combine(ConfigDir, "OptionalModules.ini");
    string[] text = System.IO.File.ReadAllLines(OptionalModulesFile);

    bool UnityOn = true;

    foreach (string line in text) {
      if (line.Contains("Unity OFF"))
      {
        UnityOn = false;
      }
    }

    if (!UnityOn) {
      Console.WriteLine("Disabling unity");
      bUseUnityBuild = false;
      bForceUnityBuild = false;
      bUseAdaptiveUnityBuild = false;
    }

    AdditionalCompilerArguments = "/wd4103";

	  // URLab 插件的 MuJoCo 依赖需要 C++17（std::byte）
	  CppStandard = CppStandardVersion.Cpp17;

	  // 排查打包版崩溃时曾临时打开（Shipping 默认不写日志）：
	  // 2026-11-08：打包版姿态与编辑器不一致，重新打开以便抓 tilt/mass/qfrc。
	  // 打开后 Engine 模块需整体重编（约 700 个编译动作），出完包可以再关掉。
	  bUseLoggingInShipping = true;
	}
}
