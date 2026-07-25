#pragma once

#include "Holodeck.h"

#include "TaskSensor.h"

#include "DistanceTask.generated.h"

/**
* UDistanceTask
* Inherits from the TaskSensor class.
* Calculates a dense distance based reward.
* Maximizes Distance if Maximize distance is set to true.
* Location is used when actor is null.
* Terminal is set to true when the agent is within its GoalDistance.
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UDistanceTask : public UTaskSensor
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	UDistanceTask() : MaximizeDistance(false), Interval(1), GoalDistance(1), GoalActor(nullptr),
		GoalLocation(this->GetComponentLocation()), DistanceActor(nullptr),
		DistanceActorTag(""), GoalActorTag("") {}

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

	// Set to true to maximize instead of minimize distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool MaximizeDistance;

	// Distance to next reward
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float Interval;

	// Required proximity for terminal
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float GoalDistance;

	// Goal actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		AActor* GoalActor;

	// Location (Used if actor is null) 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FVector GoalLocation;

	// Actor to reach goal (if null component location is used)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		AActor* DistanceActor;

protected:
	// Checkout HolodeckSensor.h for the documentation for this overridden function.
	virtual void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float ComputeNextDistance();
	float CalcDistance();
	float NextDistance;
	FString DistanceActorTag;
	FString GoalActorTag;
	bool UseZ = false;
};
