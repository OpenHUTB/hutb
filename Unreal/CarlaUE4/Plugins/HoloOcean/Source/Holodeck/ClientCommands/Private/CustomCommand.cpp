// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckGameMode.h"

void UCustomCommand::Execute() {
	UE_LOG(LogHolodeck, Log, TEXT("Executing CustomCommand."));
	if (StringParams.size() < 1) {
		UE_LOG(LogHolodeck, Error, TEXT("No command name found in CustomCommand call. Command not excecuted."));
		return;
	}
	FString name(StringParams[0].c_str());
	TArray<float> nums;
	for (float f : NumberParams) {
		nums.Add(f);
	}
	TArray<FString> strs;
	for (unsigned int i = 1; i < StringParams.size(); i++) {
		std::string s = StringParams[i];
		strs.Add(FString(s.c_str()));
	}

	AHolodeckGameMode* Game = static_cast<AHolodeckGameMode*>(Target);
	Game->ExecuteCustomCommand(name, nums, strs);
}