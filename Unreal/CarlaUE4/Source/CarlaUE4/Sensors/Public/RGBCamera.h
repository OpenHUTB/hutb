#pragma once

#include "Holodeck.h"

#include "HolodeckCamera.h"
#include "RGBCamera.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API URGBCamera : public UHolodeckCamera {
	GENERATED_BODY()

public:
	URGBCamera();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

	UPROPERTY(EditAnywhere)
	int TicksPerCapture = 1;

protected:
	//Checkout HolodeckSensor.h for the documentation on these overridden functions.
	virtual void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction);

	virtual int GetNumItems() { return CaptureWidth * CaptureHeight; };
	virtual int GetItemSize() { return sizeof(float); };

private:
	int TickCounter = 0;

};
