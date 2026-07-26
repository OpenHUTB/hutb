// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckGameMode.h"
#include "AdjustRenderQualityCommand.h"

void UAdjustRenderQualityCommand::Execute() {
	UE_LOG(LogHolodeck, Log, TEXT("UAdjustRenderQualityCommand::Execute change render quality"));

	if (StringParams.size() != 0 || NumberParams.size() != 1) {
		UE_LOG(LogHolodeck, Error, TEXT("Unexpected argument length found in UAdjustRenderQualityCommand. Command not executed."));
		return;
	}

	UGameUserSettings* settings = GEngine->GetGameUserSettings();

	// 0 = worst quality, 3 = best
	// see 
	int quality = NumberParams[0];

	if (quality < 0 || quality > 3)	{
		UE_LOG(LogHolodeck, Warning, TEXT("Invalid quality passed to UAdjustRenderQualityCommand!"));
		return;
	}

	settings->SetOverallScalabilityLevel(quality);

	if (quality == 0) {
		// quality 0 will set the ScreenPercentage to 50. If the users wants to lower the resolution of the viewport or camera,
		// there are better ways of doing so, so force the scale to be 1 (100%)
		settings->SetResolutionScaleNormalized(1.0f);
	}

	// Apply the settings (these function calls are what ApplySettings() does) without writing the config out to disk
	// this is to prevent changes from persisting between holodeck instances
	settings->ApplyResolutionSettings(false);
	settings->ApplyNonResolutionSettings();

	UE_LOG(LogHolodeck, Log, TEXT("UAdjustRenderQualityCommand was successful"));
}
