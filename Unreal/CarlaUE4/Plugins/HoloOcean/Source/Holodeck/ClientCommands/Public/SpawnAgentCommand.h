#pragma once

#include "Holodeck.h"

#include "Command.h"
#include "SpawnAgentCommand.generated.h"

/**
  * SpawnAgentCommand
  * The command used to spawn agents in the world.
  * 
  * StringParameters are expected to be AgentType, and then Name.
  * NumberParameters are expected to be X, Y, then Z coords of the location to spawn at.
  * 
  * To enable it to spawn other agents you will need to: 
  * 1. Go to the HolodeckGameModeBlueprint in editor
  * 2. Go to the SpawnAgent Function and select the AgentBpMap data field
  * 3. Add the agent's name and bp reference to the map
  */
UCLASS(ClassGroup = (Custom))
class HOLODECK_API USpawnAgentCommand : public UCommand {
	GENERATED_BODY()
public:
	//See UCommand for the documentation of this overridden function. 
	void Execute() override;
};
