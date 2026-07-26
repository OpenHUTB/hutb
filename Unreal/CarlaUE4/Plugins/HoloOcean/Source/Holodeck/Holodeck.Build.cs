// MIT License (c) 2019 BYU PCCL see LICENSE file

using UnrealBuildTool;

public class Holodeck : ModuleRules
{
	public Holodeck(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivatePCHHeaderFile = "Holodeck.h";
        PrivateIncludePaths.AddRange(new [] {
             "Holodeck/Agents/Public",
             "Holodeck/General/Public",
             "Holodeck/Sensors/Public",
             "Holodeck/Utils/Public",
             "Holodeck/HolodeckCore/Public",
             "Holodeck/ClientCommands/Public",
             "Holodeck/Tasks/Public"
         });

        PublicDependencyModuleNames.AddRange(new [] {"ApplicationCore", "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "SlateCore", "Slate", "PhysX", "APEX", "Json", "JsonUtilities", "RenderCore", "RHI", "Landscape" });

#if PLATFORM_LINUX
        PublicDependencyModuleNames.AddRange(new [] { "rt", "pthread" };
        //TARGET_LINK_LIBRARIES(UHolodeckServer rt pthread)
#endif
	}
}
