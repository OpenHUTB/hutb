// MIT License (c) 2021 BYU FRoStLab see LICENSE file
#pragma once

#include "Holodeck.h"
#include "HolodeckSensor.h"

#include <limits>
#include "Kismet/KismetMathLibrary.h"

#include "AcousticBeaconSensor.generated.h"

/**
  * AcousticBeaconSensor
  * Inherits from the HolodeckSensor class
  * Check out the parent class for documentation on all of the overridden funcions. 
  * Gets the true velocity of the component that the sensor is attached to. 
  */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UAcousticBeaconSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:
	/**
	  * Default Constructor
	  */
	UAcousticBeaconSensor();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;
	UAcousticBeaconSensor* fromSensor = NULL;

protected:
	//See HolodeckSensor for the documentation of these overridden functions.
	int GetNumItems() override { return 4; };
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/**
	  * Parent
	  * After initialization, Parent contains a pointer to whatever the sensor is attached to.
	  * Not owned.
	  */
	UPrimitiveComponent* Parent;
	float WaitBuffer[4];
	int WaitTicks = -1;
	float SpeedOfSound = 1500;
};
