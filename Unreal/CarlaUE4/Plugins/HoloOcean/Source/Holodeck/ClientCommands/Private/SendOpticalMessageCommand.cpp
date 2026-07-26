// MIT License (c) 2021 BYU FRoStLab see LICENSE file
#include "Holodeck.h"
#include "SendOpticalMessageCommand.h"
#include "OpticalModemSensor.h"
#include "HolodeckGameMode.h"

void USendOpticalMessageCommand::Execute() {

	UE_LOG(LogHolodeck, Log, TEXT("SendOpticalMessageCommand::Execute"));

	// Verify things and get everything set up 
	verifyf(StringParams.size() == 4 && NumberParams.size() == 0, TEXT("USendOpticalMessageCommand::Execute: Invalid Arguments"));
	AHolodeckGameMode* GameTarget = static_cast<AHolodeckGameMode*>(Target);
	verifyf(GameTarget != nullptr, TEXT("%s UCommand::Target is not a UHolodeckGameMode*."), *FString(__func__));
	UWorld* World = Target->GetWorld();
	verify(World);

	// Get sensor it came from
	FString FromAgentName = StringParams[0].c_str();
	FString FromSensorName = StringParams[1].c_str();

	AHolodeckAgent* FromAgent = GetAgent(FromAgentName);
	verifyf(FromAgent, TEXT("%s Could not find agent %s"), *FString(__func__), *FromAgentName);
	verifyf(FromAgent->SensorMap.Contains(FromSensorName), TEXT("%s Sensor %s not found on agent %s"), *FString(__func__), *FromSensorName, *FromAgentName);
	UOpticalModemSensor* FromSensor = (UOpticalModemSensor*)FromAgent->SensorMap[FromSensorName];

	// Get sensor where it's going
	FString ToAgentName = StringParams[2].c_str();
	FString ToSensorName = StringParams[3].c_str();

	AHolodeckAgent* ToAgent = GetAgent(ToAgentName);
	verifyf(ToAgent, TEXT("%s Could not find agent %s"), *FString(__func__), *ToAgentName);
	verifyf(ToAgent->SensorMap.Contains(ToSensorName), TEXT("%s Sensor %s not found on agent %s"), *FString(__func__), *ToSensorName, *ToAgentName);
	UOpticalModemSensor* ToSensor = (UOpticalModemSensor*)ToAgent->SensorMap[ToSensorName];

	// Send the message
	ToSensor->FromSensor = FromSensor;
}