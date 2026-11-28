// Copyright (c) OpenHUTB. All rights reserved.

using UnrealBuildTool;

public class UnderwaterScene : ModuleRules
{
    public UnderwaterScene(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });
    }
}
