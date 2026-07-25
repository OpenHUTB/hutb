// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "HolodeckCore/Public/HolodeckSonar.h"

#include "GenericPlatform/GenericPlatformMath.h"
#include "Octree.h"
#include "Kismet/KismetMathLibrary.h"
#include "Async/ParallelFor.h"
#include "MultivariateNormal.h"

#include "Json.h"

#include "SidescanSonar.generated.h"

#define Pi 3.1415926535897932384626433832795
/**
 * USidescanSonar
 */
UCLASS()
class HOLODECK_API USidescanSonar : public UHolodeckSonar
{
    GENERATED_BODY()

public:
    /*
    * Default Constructor
    */
   USidescanSonar();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

	/*
	* Cleans up octree
	*/
	virtual void BeginDestroy() override;

protected:
	//See HolodeckSensor for the documentation of these overridden functions.
	int GetNumItems() override { return RangeBins; }; // Returns 1D array for buffer for Sidescan Sonar
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere)
	int32 RangeBins = 0;

	UPROPERTY(EditAnywhere)
	float RangeRes = 0;

	UPROPERTY(EditAnywhere)
	int32 AzimuthBins = 0;

	UPROPERTY(EditAnywhere)
	float AzimuthRes = 0;

	UPROPERTY(EditAnywhere)
	int32 ElevationBins = 0;

	UPROPERTY(EditAnywhere)
	float ElevationRes = 0;

private:
	/*
	 * Parent
	 * After initialization, Parent contains a pointer to whatever the sensor is attached to.
	 */
	AActor* Parent;

	// Used for counting how many leaves in a bin for averaging at the end
	int32* count;
	uint32 runtickCounter = 0;
	
	// for adding noise
	MultivariateNormal<1> addNoise;
	MultivariateNormal<1> multNoise;
};
