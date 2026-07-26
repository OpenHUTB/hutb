// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "TaskSensor.h"
#include "CupGameTask.generated.h"

/**
* UCupGameTask
* Inherits from the TaskSensor class.
* Initializes the cup game in the world and calculates reward based off of which cup is selected
* A reward of 1 is given for one tick if the correct cup is touched and no other cups are touched,
* A reward of 2 and terminal is given when the ball itself is touched and no incorrect cups are touched.
* A reward of -1 and terminal is given when an incorrect cup is touched.
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UCupGameTask : public UTaskSensor
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

	// Keeps task from giving constant reward when the correct cup is touched.
	bool MinRewardGiven = false;
};
