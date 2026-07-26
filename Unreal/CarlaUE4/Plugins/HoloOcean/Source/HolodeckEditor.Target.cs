// MIT License (c) 2019 BYU PCCL see LICENSE file

using UnrealBuildTool;
using System.Collections.Generic;

public class HolodeckEditorTarget : TargetRules
{
	public HolodeckEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
        // LaunchModuleName = "Holodeck";
        ExtraModuleNames.Add("Holodeck");
    }

	//
	// TargetRules interface.
	//
    /*
	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutExtraModuleNames.AddRange( new string[] { "Holodeck" } );
	}
    */
}
