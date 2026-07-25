// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "HolodeckCore/Public/HolodeckSonar.h"

#include "GenericPlatform/GenericPlatformMath.h"
#include "Octree.h"
#include "Kismet/KismetMathLibrary.h"
#include "Async/ParallelFor.h"
#include "MultivariateNormal.h"
#include "MultivariateUniform.h"

#include "Json.h"

#include "SinglebeamSonar.generated.h"

#define Pi 3.1415926535897932384626433832795
/**
 * USinglebeamSonar
 */
UCLASS()
class HOLODECK_API USinglebeamSonar : public UHolodeckSonar
{
	GENERATED_BODY()
	
public:
	/*
	* Default Constructor
	*/
	USinglebeamSonar();

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
	int GetNumItems() override { return RangeBins; };
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void showRegion(float DeltaTime) override;

	virtual bool inRange(Octree* tree) override;
	
	UPROPERTY(EditAnywhere)
	float OpeningAngle = 30;

	UPROPERTY(EditAnywhere)
	int32 RangeBins = 0;

	UPROPERTY(EditAnywhere)
	float RangeRes = 0;

	UPROPERTY(EditAnywhere)
	int32 CentralAngleBins = 0; 

	UPROPERTY(EditAnywhere)
	float CentralAngleRes = 0;

	UPROPERTY(EditAnywhere)
	int32 OpeningAngleBins = 0; 

	UPROPERTY(EditAnywhere)
	float OpeningAngleRes = 0;

private:
	/*
	 * Parent
	 * After initialization, Parent contains a pointer to whatever the sensor is attached to.
	 */
	AActor* Parent;

	// angles unique to Singlebeam
	float CentralAngle = 360;

	float minOpeningAngle;
	float maxOpeningAngle;
	float minCentralAngle;
	float maxCentralAngle;
	
	// various computations we want to cache
	float sqrt3_2;
	float sinOffset;

	// Used to hold leafs when parallelized sorting/binning happens
	int32* count;
	
	// for adding noise
	MultivariateNormal<1> addNoise;
	MultivariateNormal<1> multNoise;
	MultivariateUniform<1> rNoise;
};
