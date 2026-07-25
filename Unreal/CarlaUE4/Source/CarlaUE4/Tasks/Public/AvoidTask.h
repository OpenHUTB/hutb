#pragma once

#include "Holodeck.h"

#include "TaskSensor.h"

#include "AvoidTask.generated.h"

/**
* UAvoidTask
* Inherits from the TaskSensor class.
* Calculates Avoid reward based on distance and line of sight.
* If OnlyWithinSight is true, the reward is set to the percent distance covered
* from the MinDistance to the ToAvoid Target if the angle from the agent to the Target
* is less than FOVRadians and is there is nothing blocking the agent's line of sight,
* otherwise the reward is 0.
* If OnlywithinSight is false, the reward is set to the percent distance covered
* from the MinDistance to the ToAvoid Actor.
* Terminal is always false.
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UAvoidTask : public UTaskSensor
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	UAvoidTask() : ToAvoid(nullptr), StartSocket(""), EndSocket(""),
		OnlyWithinSight(true), FOVRadians(1.5), MinDistance(10000), ToAvoidTag("") {}

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

	// Actor to Avoid
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		AActor* ToAvoid;

	// Socket on Avoid actor for ray trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString StartSocket;

	// Socket on Avoid actor for ray trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString EndSocket;

	// Only give reward if target is in sight
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool OnlyWithinSight;

	// Defines the agent's field of view
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float FOVRadians;

	// Defines the minimum distance to recieve positive reward
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float MinDistance;

protected:
	//Checkout HolodeckSensor.h for the documentation for this overridden function.
	virtual void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Scales score between 0-1 to 0-100
	const int MaxScore = 100;

	FString ToAvoidTag;
};
