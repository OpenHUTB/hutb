#pragma once

#include "Holodeck.h"

#include "Command.h"
#include "RenderViewportCommand.generated.h"

/**
* RenderViewportCommand
* Command used to toggle rendering the viewport
*
* StringParameters are expected to be empty.
* NumberParameters are expected to be 1 to enable rendering and 0 to disable rendering.
* 
*/
UCLASS(ClassGroup = (Custom))
class HOLODECK_API URenderViewportCommand : public UCommand
{
	GENERATED_BODY()

public:
	void Execute() override;
};
