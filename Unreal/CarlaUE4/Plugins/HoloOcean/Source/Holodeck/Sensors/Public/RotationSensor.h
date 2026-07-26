// MIT License (c) 2019 BYU PCCL see LICENSE file
#pragma once

#include "Holodeck.h"

#include "HolodeckSensor.h"

#include "RotationSensor.generated.h"

/**
* RotationSensor
* Inherits from the HolodeckSensor class
* Check out the parent class for documentation on all of the overridden funcions.
* Gets the true rotation of the component that the sensor is attached to.
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API URotationSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	URotationSensor	();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

protected:
	//See HolodeckSensor for the documentation of these overridden functions.
	int GetNumItems() override { return 3; };
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/**
	* Parent
	* After initialization, Parent contains a pointer to whatever the sensor is attached to.
	*/
	AActor* Parent;

};
