// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "Holodeck.h"
#include "Android.h"
#include "HandAgent.h"

#include "Components/SceneComponent.h"
#include "HolodeckSensor.h"

#include "RelativeSkeletalPositionSensor.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HOLODECK_API URelativeSkeletalPositionSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:	
	/**
	  * Default Constructor.
	  */
	URelativeSkeletalPositionSensor();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

protected:
	int GetNumItems() override;
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	USkeletalMeshComponent* SkeletalMeshComponent;
	TArray<FName> Bones;
	TArray<FName> ParentBones;
};
