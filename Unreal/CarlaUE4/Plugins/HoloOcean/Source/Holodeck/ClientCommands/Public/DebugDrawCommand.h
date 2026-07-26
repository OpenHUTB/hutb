#pragma once

#include "Holodeck.h"

#include "Command.h"
#include "DrawDebugHelpers.h"
#include "DebugDrawCommand.generated.h"

/**
* DebugDrawCommand
* Command used to draw debug objects in the world. 
*
* StringParameters are expected to be empty.
* NumberParameters is expected to be length 12 and of the format [func_type, vector1[3], vector2[3], color[3], thickness/size, lifetime]
*/
UCLASS(ClassGroup = (Custom))
class HOLODECK_API UDebugDrawCommand : public UCommand
{
	GENERATED_BODY()


public:
	//See UCommand for the documentation of this overridden function. 
	void Execute() override;

private:

};
