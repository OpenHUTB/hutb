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

#include <numeric>
#include "Json.h"

#include "ImagingSonar.generated.h"

#define Pi 3.1415926535897932384626433832795
/**
 * UImagingSonar
 */
UCLASS()
class HOLODECK_API UImagingSonar : public UHolodeckSonar
{
	GENERATED_BODY()
	
public:
	/*
	* Default Constructor
	*/
	UImagingSonar();

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
	int GetNumItems() override { return RangeBins*AzimuthBins; };
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

	UPROPERTY(EditAnywhere)
	bool MultiPath = false;

	UPROPERTY(EditAnywhere)
	int32 ClusterSize = 5;

	UPROPERTY(EditAnywhere)
	bool ScaleNoise = true;

	UPROPERTY(EditAnywhere)
	int32 AzimuthStreaks = 0;

private:
	/*
	 * Parent
	 * After initialization, Parent contains a pointer to whatever the sensor is attached to.
	 */
	AActor* Parent;

	// various computations we want to cache
	int32 AzimuthBinScale = 1;
	float perfectCos;

	// Used to hold leaves for multipath
	TMap<FIntVector,Octree*> mapLeaves;
	TMap<FIntVector,Octree*> mapSearch;
	TArray<TArray<Octree*>> cluster;
	int32* count;
	int32* hasPerfectNormal;
	
	// for adding noise
	MultivariateNormal<1> addNoise;
	MultivariateNormal<1> multNoise;
	MultivariateUniform<1> rNoise;
};
