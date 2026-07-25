// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "HolodeckSensor.h"
#include "Android.h"
#include "HandAgent.h"

#include "Components/SceneComponent.h"

#include "PressureSensor.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
/**
* UPressureSensor
*/
class HOLODECK_API UPressureSensor : public UHolodeckSensor
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPressureSensor();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;
	
	/**
	 * Callback function that is called whenever part of the actor is hit. 
	 */
	UFUNCTION()
	void OnHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	virtual void BeginDestroy() override;

private:

	float *PrivateData;
	int NumJoints;
	FName *Joints;

protected:
	// 94 DOF each with 2 length 3 vectors containing impulse normal info and hit location
	int GetNumItems() override;
	int GetItemSize() override { return sizeof(float); };

	// Called every frame
	virtual void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float* AddHitToBuffer(FString BoneName, FVector HitBoneLocation, FVector NormalImpulse, float* Data);

	AActor* Parent;


	USkeletalMeshComponent* SkeletalMeshComponent;

	void InitJointMap();
	TMap<FString, int> JointMap;
};
