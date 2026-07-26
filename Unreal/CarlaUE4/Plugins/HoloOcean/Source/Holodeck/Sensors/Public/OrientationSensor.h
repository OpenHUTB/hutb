// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "Holodeck.h"

#include "HolodeckPawnController.h"
#include "HolodeckSensor.h"

#include "OrientationSensor.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )

/**
* UOrientationSensor
* Inherits from the HolodeckSensor class
* Check out the parent class for documentation on all of the overridden funcions.
* Gives the complete orientation of the parent agent in three vectors: forward, right, and up.
*/
class HOLODECK_API UOrientationSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:	
	/**
	  * Default Constructor.
	  */
	UOrientationSensor();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;
	
protected:
	// See HolodeckSensor for documentation on these classes.
	int GetNumItems() override { return 9; };
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPrimitiveComponent* Parent;
	UStaticMeshComponent* RootMesh;
	UWorld* World;
};
