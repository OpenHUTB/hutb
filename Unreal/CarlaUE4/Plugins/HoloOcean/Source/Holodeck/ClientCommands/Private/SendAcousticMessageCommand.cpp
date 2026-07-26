// MIT License (c) 2021 BYU FRoStLab see LICENSE file
#include "Holodeck.h"
#include "SendAcousticMessageCommand.h"
#include "AcousticBeaconSensor.h"
#include "HolodeckGameMode.h"

void USendAcousticMessageCommand::Execute() {

	UE_LOG(LogHolodeck, Log, TEXT("SendAcousticMessageCommand::Execute"));

	// Verify things and get everything set up 
	verifyf(StringParams.size() == 4 && NumberParams.size() == 0, TEXT("USendAcousticMessageCommand::Execute: Invalid Arguments"));
	AHolodeckGameMode* GameTarget = static_cast<AHolodeckGameMode*>(Target);
	verifyf(GameTarget != nullptr, TEXT("%s UCommand::Target is not a UHolodeckGameMode*."), *FString(__func__));
	UWorld* World = Target->GetWorld();
	verify(World);

	// Get sensor it came from
	FString fromAgentName = StringParams[0].c_str();
	FString fromSensorName = StringParams[1].c_str();

	AHolodeckAgent* fromAgent = GetAgent(fromAgentName);
	verifyf(fromAgent, TEXT("%s Could not find agent %s"), *FString(__func__), *fromAgentName);
	verifyf(fromAgent->SensorMap.Contains(fromSensorName), TEXT("%s Sensor %s not found on agent %s"), *FString(__func__), *fromSensorName, *fromAgentName);
	UAcousticBeaconSensor* fromSensor = (UAcousticBeaconSensor*)fromAgent->SensorMap[fromSensorName];

	// Get sensor where it's going
	FString toAgentName = StringParams[2].c_str();
	FString toSensorName = StringParams[3].c_str();

	AHolodeckAgent* toAgent = GetAgent(toAgentName);
	verifyf(toAgent, TEXT("%s Could not find agent %s"), *FString(__func__), *toAgentName);
	verifyf(toAgent->SensorMap.Contains(toSensorName), TEXT("%s Sensor %s not found on agent %s"), *FString(__func__), *toSensorName, *toAgentName);
	UAcousticBeaconSensor* toSensor = (UAcousticBeaconSensor*)toAgent->SensorMap[toSensorName];

	// Send the message
	toSensor->fromSensor = fromSensor;
}
