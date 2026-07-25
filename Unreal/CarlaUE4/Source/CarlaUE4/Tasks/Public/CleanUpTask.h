// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "TaskSensor.h"
#include "CleanUpTask.generated.h"

/**
* UCleanUpTask
* Inherits from the TaskSensor class.
* Initializes the clean up task in the world. This only works in the CleanUp world. 
* The reward is based on the number of pieces of trash placed in the trash can. 
* For each piece of trash added to the can, a reward of 1 is given. For each piece
* of trash removed, a reward of -1 is given. If all the trash is in the can, terminal
* is given.
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UCleanUpTask : public UTaskSensor
{
	GENERATED_BODY()

public:

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

protected:
	// Checkout HolodeckSensor.h for the documentation for this overridden function.
	virtual void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	int PrevTick_TrashInCan = 0;
	int TotalTrash = 0;
};