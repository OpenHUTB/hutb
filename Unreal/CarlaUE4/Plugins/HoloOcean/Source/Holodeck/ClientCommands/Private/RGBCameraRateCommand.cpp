#include "Holodeck.h"
#include "RGBCameraRateCommand.h"
#include "RGBCamera.h"
#include "HolodeckGameMode.h"

void URGBCameraRateCommand::Execute() {

	UE_LOG(LogHolodeck, Log, TEXT("RGBCameraRateCommand::Execute"));

	verifyf(StringParams.size() == 2 && NumberParams.size() == 1, TEXT("URGBCameraRateCommand::Execute: Invalid Arguments"));

	AHolodeckGameMode* GameTarget = static_cast<AHolodeckGameMode*>(Target);

	verifyf(GameTarget != nullptr, TEXT("%s UCommand::Target is not a UHolodeckGameMode*."), *FString(__func__));

	UWorld* World = Target->GetWorld();
	verify(World);

	FString AgentName = StringParams[0].c_str();
	int ticksPerCapture = NumberParams[0];

	verifyf(ticksPerCapture > 0, TEXT("%s Invalid ticks per capture provided!"), *FString(__func__));

	AHolodeckAgent* Agent = GetAgent(AgentName);

	verifyf(Agent, TEXT("%s Could not find agent %s"), *FString(__func__), *AgentName);

	FString SensorName = StringParams[1].c_str();

	verifyf(Agent->SensorMap.Contains(SensorName), TEXT("%s Sensor %s not found on agent %s"), *FString(__func__), *SensorName, *AgentName);

	URGBCamera* Camera = (URGBCamera*)Agent->SensorMap[SensorName];
	Camera->TicksPerCapture = ticksPerCapture;
}
