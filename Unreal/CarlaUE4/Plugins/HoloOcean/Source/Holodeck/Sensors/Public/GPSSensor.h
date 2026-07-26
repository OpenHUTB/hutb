// MIT License (c) 2021 BYU FRoStLab see LICENSE file
#pragma once

#include "Holodeck.h"

#include "HolodeckSensor.h"

#include <limits>
#include "MultivariateNormal.h"

#include "GPSSensor.generated.h"

/**
* GPSSensor
* Inherits from the HolodeckSensor class
* Check out the parent class for documentation on all of the overridden functions.
* Reports the XYZ coordinate of the parent agent. 
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UGPSSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:
	/*
	* Default Constructor
	*/
	UGPSSensor();

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
	int GetNumItems() override { return 3; };
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere)
	float GPSDepth = 2;

private:
	/*
	 * Parent
	 * After initialization, Parent contains a pointer to whatever the sensor is attached to.
	 */
	USceneComponent* Parent;
	MultivariateNormal<3> mvn;
	MultivariateNormal<1> depthMVN;
};
