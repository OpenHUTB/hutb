#pragma once

#include "Holodeck.h"

#include "HolodeckSensor.h"
#include "Command.h"
#include "RotateSensorCommand.generated.h"

/**
* RotateSensorCommand
* Command used to rotate a sensor on an agent
*
* StringParameters expect two arguments, the agent and sensor names.
* NumberParameters expect three arguments, representing the rotation
*/
UCLASS()
class HOLODECK_API URotateSensorCommand : public UCommand
{
	GENERATED_BODY()
	
	public:
	//See UCommand for the documentation of this overridden function.
	void Execute() override;
	
	
};
