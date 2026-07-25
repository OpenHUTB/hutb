#include "Holodeck.h"
#include "Command.h"
#include "HolodeckGameMode.h"

UCommand::UCommand() {
	
}

void UCommand::Init(const std::vector<float>& NumberParameters, const std::vector<std::string>& StringParameters, AActor* const ParameterTarget) {
	this->NumberParams = NumberParameters;
	this->StringParams = StringParameters;
	this->Target = ParameterTarget;
}

AHolodeckAgent* UCommand::GetAgent(FString AgentName) {

	AHolodeckGameMode* GameTarget = static_cast<AHolodeckGameMode*>(Target);
	UHolodeckServer* Server = GameTarget->GetAssociatedServer();
	if (Server->AgentMap.Contains(AgentName)) {
		return Server->AgentMap[AgentName];
	} else {
		UE_LOG(LogHolodeck, Error, TEXT("Unable to find the agent %s"), *AgentName);
		return nullptr;
	}
}
