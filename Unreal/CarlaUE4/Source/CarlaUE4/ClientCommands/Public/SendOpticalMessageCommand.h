// MIT License (c) 2021 BYU FRoStLab see LICENSE file
#pragma once

#include "Holodeck.h"

#include "Command.h"
#include "SendOpticalMessageCommand.generated.h"

/**
* SendOpticalMessageCommand
* 
*/
UCLASS(ClassGroup = (Custom))
class HOLODECK_API USendOpticalMessageCommand : public UCommand {
	GENERATED_BODY()
public:
	//See UCommand for the documentation of this overridden function. 
	void Execute() override;
};
