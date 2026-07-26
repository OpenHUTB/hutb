// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckGameMode.h"
#include "RenderViewportCommand.h"

void URenderViewportCommand::Execute() {
	UE_LOG(LogHolodeck, Log, TEXT("URenderViewportCommand::Execute toggling viewport rendering"));

	if (StringParams.size() != 0 || NumberParams.size() != 1) {
		UE_LOG(LogHolodeck, Error, TEXT("Unexpected argument length found in URenderViewportCommand. Command not executed."));
		return;
	}

	bool succeeded;

	// TODO: Apparently you aren't "supposed" to just manually send a command like this.
	//       It would be better to get a reference to the ViewFamily to do `ViewFamily.EngineShowFlags.Rendering = 0`;
	//		 This is basically what the command below does.

	const FString command = TEXT("ShowFlag.Rendering ");
	const FString paramater = (NumberParams[0] == 1) ? TEXT("2") : TEXT("0");
	const FString complete_command = command + paramater;

	succeeded = GEngine->Exec(GetWorld(), *complete_command);

	if (succeeded) {
		UE_LOG(LogHolodeck, Log, TEXT("URenderViewportCommand was successful"));
	} else {
		UE_LOG(LogHolodeck, Log, TEXT("URenderViewportCommand failed!"));
	}

}
