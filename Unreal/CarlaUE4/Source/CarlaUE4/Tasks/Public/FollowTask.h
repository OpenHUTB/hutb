#pragma once

#include "Holodeck.h"

#include "TaskSensor.h"

#include "FollowTask.generated.h"

/**
* UFollowTask
* Inherits from the TaskSensor class.
* Calculates follow reward based on distance and line of sight.
* If OnlyWithinSight is true, the reward is set to the percent distance covered 
* from the MinDistance to the ToFollow Target if the angle from the agent to the Target
* is less than FOVRadians and is there is nothing blocking the agent's line of sight, 
* otherwise the reward is 0.
* If OnlywithinSight is false, the reward is set to the percent distance covered 
* from the MinDistance to the ToFollow Actor.
* Terminal is always false.
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UFollowTask : public UTaskSensor
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	UFollowTask() : ToFollow(nullptr), FollowSocket(""), OnlyWithinSight(true),
		FOVRadians(1.5), MinDistance(10000), ToFollowTag("") {}

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

	// Actor to follow
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		AActor* ToFollow;

	// Socket on Follow actor for ray trace
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString FollowSocket;

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

	FString ToFollowTag;
};
