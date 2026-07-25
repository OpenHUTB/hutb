// MIT License (c) 2019 BYU PCCL see LICENSE file
#pragma once

#include "Holodeck.h"

#include "Components/SceneComponent.h"
#include "HolodeckSensor.h"

#include "CollisionSensor.generated.h"

/**
* UCollisionSensor
* Inherits from the HolodeckSensor class
* Check out the parent class for documentation on all of the overridden funcions.
* Reports whether the parent agent is currenty colliding with any other object. 
*/
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UCollisionSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	UCollisionSensor();
	
	/**
	* InitializeComponent
	* Called on level load, before begin play
	* It's important for these things to be initialized before the its actor's BeginPlay() is called.
	*/
	void InitializeComponent() override;

	UFUNCTION()
		void OnHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

protected:
	//See HolodeckSensor for the documentation of these overridden functions.
	int GetNumItems() override { return 1; };
	int GetItemSize() override { return sizeof(bool); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/**
	* Parent
	* After initialization, Parent contains a pointer to whatever the sensor is attached to.
	* Not owned.
	*/
	AActor* Parent;

	bool bIsColliding = false;

};
