// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MujocoTestTarget : TargetRules
{
	public MujocoTestTarget(TargetInfo Target) : base(Target)
	{
        CppStandard = CppStandardVersion.Cpp17;
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
		// IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MujocoTest");
	}
}
