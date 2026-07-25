// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "ClientCommands/Public/Command.h"
#include "CustomCommand.generated.h"

/**
* CustomCommand
* This command is inherited by a blueprint. It is to be used to easily implement simple commands for
* any number of different functions.
*
* StringParameters expect at least 1 argument as the name of the command and any number of additional string params.
* NumberParameters of any size.
*/
UCLASS()
class HOLODECK_API UCustomCommand : public UCommand
{
	GENERATED_BODY()

public:
	//See UCommand for the documentation of this overridden function. 
	void Execute() override;
};
