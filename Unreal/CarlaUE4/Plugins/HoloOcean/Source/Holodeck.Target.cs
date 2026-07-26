// MIT License (c) 2019 BYU PCCL see LICENSE file

using UnrealBuildTool;
using System;
using System.Collections.Generic;

public class HolodeckTarget : TargetRules
{
    public HolodeckTarget(TargetInfo Target) : base(Target)
    {
		Type = TargetType.Game;
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