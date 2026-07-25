#pragma once

#include "Holodeck.h"

#include <vector>
#include <map>

#include "Command.h"
#include "SpawnAgentCommand.h"
#include "TeleportCameraCommand.h"
#include "RGBCameraRateCommand.h"
#include "DebugDrawCommand.h"
#include "RenderViewportCommand.h"
#include "AdjustRenderQualityCommand.h"
#include "CustomCommand.h"
#include "AddSensorCommand.h"
#include "RemoveSensorCommand.h"
#include "RotateSensorCommand.h"
#include "SendAcousticMessageCommand.h"
#include "SendOpticalMessageCommand.h"

#include "CommandFactory.generated.h"

class AHolodeckGameMode;

/**
  * UCommandFactory
  * This is the class that should be used to instantiate UCommand objects. Feed it the name of the command along with
  * the parameters that the command will need to execute. If the parameters are not needed, then give it nullptr or
  * empty vectors and it will work fine.
  * The purpose of this was to separate knowledge of specific commands from the command center, to remove circular
  * dependencies, and to give an easy way of spawning commands.
  * When you make a new command, make sure to add it to the CommandMap in the MakeCommand function in the cpp file
  */
UCLASS(ClassGroup = (Custom), abstract)
class HOLODECK_API UCommandFactory : public UObject {
	GENERATED_BODY()

	typedef std::map<std::string, UCommand*(*)()> UCommandMapType;

public:
	/**
	  * MakeCommand
	  * This is the factory method for producing commands.
	  */
	static UCommand* MakeCommand(const std::string& Name, const std::vector<float>& NumberParameters, const std::vector<std::string>& StringParameters, AActor* ParameterGameMode);

private:
	/**
	  * UCommandFactory
	  * Default constructor. Should not be instantiated, hence it is private.
	  */
	UCommandFactory() {};

	template<typename T>
	static UCommand* CreateInstance() { return NewObject<T>(); }
};
