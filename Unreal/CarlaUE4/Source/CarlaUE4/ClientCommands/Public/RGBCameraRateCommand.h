#pragma once

#include "Holodeck.h"

#include "Command.h"
#include "RGBCameraRateCommand.generated.h"

/**
* RGBCameraRateCommand
* 
*/
UCLASS(ClassGroup = (Custom))
class HOLODECK_API URGBCameraRateCommand : public UCommand {
	GENERATED_BODY()
public:
	//See UCommand for the documentation of this overridden function. 
	void Execute() override;
};
