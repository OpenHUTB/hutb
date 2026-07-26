#pragma once

#include "Holodeck.h"

#include "Command.h"
#include "AdjustRenderQualityCommand.generated.h"

/**
* AdjustRenderQualityCommand
* The command used to alter the fidelity of the simulation.
*
* StringParameters are expected to be empty.
* NumberParameters are expected to be an integer between 0 and 3 inclusive
*
*/
UCLASS(ClassGroup = (Custom))
class HOLODECK_API UAdjustRenderQualityCommand : public UCommand
{
	GENERATED_BODY()

public:
	// See UCommand for the documentation of this overridden function. 
	void Execute() override;
};
