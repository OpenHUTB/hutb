#pragma once

#include "Holodeck.h"

#include "Command.h"
#include "TeleportCameraCommand.generated.h"

/**
* TeleportCameraCommand
* The command used to change the location and rotation of the camera
*
* StringParameters are expected to be empty.
* NumberParameters are expected to have 6 entries, the first three representing the location and the last three representing
* the rotation
*
*/
UCLASS()
class HOLODECK_API UTeleportCameraCommand : public UCommand
{
	GENERATED_BODY()

public:
	//See UCommand for the documentation of this overridden function. 
	void Execute() override;


};
