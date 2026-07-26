// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckGameMode.h"
#include "RotateSensorCommand.h"

void URotateSensorCommand::Execute() {
	UE_LOG(LogHolodeck, Log, TEXT("URotateSensorCommand::Execute sensor"));

	if (StringParams.size() != 2 || NumberParams.size() != 3) {
		UE_LOG(LogHolodeck, Error, TEXT("Unexpected argument length found in URotateSensorCommand. Command not executed."));
		return;
	}

	FString AgentName = StringParams[0].c_str();
	FString SensorName = StringParams[1].c_str();

	AHolodeckAgent* Agent = GetAgent(AgentName);
	verifyf(Agent, TEXT("%s: Could not find an agent with that name! %s"), *FString(__func__), *AgentName);

	verifyf(Agent->SensorMap.Contains(SensorName), TEXT("%s: Could not find a sensor with that name! %s"), *FString(__func__), *SensorName);

	UHolodeckSensor* Sensor = Agent->SensorMap[SensorName];

	// Coordinates from the python side come in roll (x), pitch (y), yaw, (z) order
	float RotationRoll = NumberParams[0];
	float RotationPitch = NumberParams[1];
	float RotationYaw = NumberParams[2];

	UE_LOG(LogHolodeck, Log, TEXT("roll %d pitch %d yaw %d"), RotationRoll, RotationPitch, RotationYaw);

	Sensor->SetRelativeRotation(FRotator(RotationPitch, RotationYaw, RotationRoll));
}
