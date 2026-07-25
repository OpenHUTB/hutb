// MIT License (c) 2021 BYU FRoStLab see LICENSE file
#pragma once

#include "Holodeck.h"

#include "HolodeckSensor.h"

#include "MultivariateNormal.h"
#include "Kismet/KismetMathLibrary.h"

#include "DVLSensor.generated.h"

/**
  * DVLSensor
  * Inherits from the HolodeckSensor class
  * Check out the parent class for documentation on all of the overridden funcions. 
  * Gets the true velocity of the component that the sensor is attached to. 
  */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UDVLSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:
	/**
	  * Default Constructor
	  */
	UDVLSensor();

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
	//See HolodeckSensor for the documentation of these overridden functions.
	int GetNumItems() override { return ReturnRange ? 7 : 3; };
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere)
	bool DebugLines = false;

	UPROPERTY(EditAnywhere)
	float elevation = 90;

	UPROPERTY(EditAnywhere)
	bool ReturnRange = true;

	UPROPERTY(EditAnywhere)
	float MaxRange = 20*100;

private:
	/**
	  * Parent
	  * After initialization, Parent contains a pointer to whatever the sensor is attached to.
	  * Not owned.
	  */
	UPrimitiveComponent* Parent;

	// Used for noise
	float sinElev;
	float cosElev;
	MultivariateNormal<4> mvnVel;
	MultivariateNormal<4> mvnRange;
	TArray<TArray<float>> transform;

	// used for debugging
	TArray<FVector> directions;
};
