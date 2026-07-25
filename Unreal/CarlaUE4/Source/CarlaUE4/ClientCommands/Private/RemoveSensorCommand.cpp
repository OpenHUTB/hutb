// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckGameMode.h"
#include "RemoveSensorCommand.h"

void URemoveSensorCommand::Execute() {
	UE_LOG(LogHolodeck, Log, TEXT("URemoveSensorCommand::Remove sensor"));

	if (StringParams.size() != 2 || NumberParams.size() != 0) {
		UE_LOG(LogHolodeck, Error, TEXT("Unexpected argument length found in v. Command not executed."));
		return;
	}

	AHolodeckGameMode* GameTarget = static_cast<AHolodeckGameMode*>(Target);
	if (GameTarget == nullptr) {
		UE_LOG(LogHolodeck, Warning, TEXT("UCommand::Target is not a UHolodeckGameMode*. URemoveSensorCommand::Sensor not removed."));
		return;
	}

	UWorld* World = Target->GetWorld();
	if (World == nullptr) {
		UE_LOG(LogHolodeck, Warning, TEXT("URemoveSensorCommand::Execute found world as nullptr. Sensor not removed."));
		return;
	}

	FString AgentName = StringParams[0].c_str();
	FString SensorName = StringParams[1].c_str();
	AHolodeckAgent* Agent = GetAgent(AgentName);
	UHolodeckSensor* Sensor = Agent->SensorMap[SensorName];

	if (Sensor && Agent)
	{
		Agent->SensorMap.Remove(Sensor->SensorName);
		Sensor->UnregisterComponent();
	}
}
