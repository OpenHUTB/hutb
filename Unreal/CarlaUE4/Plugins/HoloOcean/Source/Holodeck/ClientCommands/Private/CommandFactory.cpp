#include "Holodeck.h"
#include "CommandFactory.h"
#include "HolodeckGameMode.h"

const static std::string SPAWN_AGENT = "SpawnAgent";

UCommand* UCommandFactory::MakeCommand(const std::string& Name, const std::vector<float>& NumberParameters, const std::vector<std::string>& StringParameters, AActor* ParameterGameMode) {
	static UCommandMapType CommandMap = { { "SpawnAgent", &CreateInstance<USpawnAgentCommand>},
										  { "TeleportCamera", &CreateInstance<UTeleportCameraCommand> },
										  { "RGBCameraRate", &CreateInstance<URGBCameraRateCommand> },
										  { "AdjustRenderQuality", &CreateInstance<UAdjustRenderQualityCommand> },
										  { "DebugDraw", &CreateInstance<UDebugDrawCommand> },
										  { "RenderViewport", &CreateInstance<URenderViewportCommand> },
										  { "AddSensor", &CreateInstance<UAddSensorCommand> },
										  { "RemoveSensor", &CreateInstance<URemoveSensorCommand> },
										  { "RotateSensor", &CreateInstance<URotateSensorCommand> },
										  { "CustomCommand", &CreateInstance<UCustomCommand> },
										  { "SendAcousticMessage", &CreateInstance<USendAcousticMessageCommand> },
										  { "SendOpticalMessage", &CreateInstance<USendOpticalMessageCommand> }, };

	UCommand*(*CreateCommandFunction)()  = CommandMap[Name];
	UCommand* ToReturn = nullptr;
	if (CreateCommandFunction) {
		ToReturn = CreateCommandFunction();
		ToReturn->Init(NumberParameters, StringParameters, ParameterGameMode);
	} else {
		UE_LOG(LogHolodeck, Warning, TEXT("CommandFactory failed to make command:  %s"), UTF8_TO_TCHAR(Name.c_str()));
	}
	return ToReturn;
}
