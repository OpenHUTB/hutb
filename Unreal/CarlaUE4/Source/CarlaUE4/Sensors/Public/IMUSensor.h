#pragma once

#include "Holodeck.h"

#include "HolodeckSensor.h"

#include "MultivariateNormal.h"

#include "IMUSensor.generated.h"

/**
  * An intertial measurement unit.
  * Returns a 1D numpy array of:
  * `[acceleration_x, acceleration_y, acceleration_z, velocity_roll, velocity_pitch, velocity_yaw]`
  */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HOLODECK_API UIMUSensor : public UHolodeckSensor {
	GENERATED_BODY()

public:
	/**
	  * Default Constructor.
	  */
	UIMUSensor();

	/**
	* InitializeSensor
	* Sets up the class
	*/
	virtual void InitializeSensor() override;

	/**
	  * GetAccelerationVector
	  * Gets the acceleration vector.
	  * @return the acceleration vector.
	  */
	FVector GetAccelerationVector();

	/**
	  * GetAngularVelocityVector
	  * Gets the angular velocity vector.
	  * @return the angular velocity vector.
	  */
	FVector GetAngularVelocityVector();

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

protected:
	// See HolodeckSensor for more information on these overridden functions.
	int GetNumItems() override { return ReturnBias ? 12 : 6; };
	int GetItemSize() override { return sizeof(float); };
	void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere)
	bool ReturnBias = false;

private:
	/**
	  * CalculateAccelerationVector
	  * Calculates the acceleration vector.
	  * @param DeltaTime the time that has passsed since the last tick.
	  */
	void CalculateAccelerationVector(float DeltaTime);

	/**
	* CalculateAngularVelocityVector
	* Calculates the angular velocity vector.
	*/
	void CalculateAngularVelocityVector();

	UPrimitiveComponent* Parent;

	UWorld* World;
	AWorldSettings* WorldSettings;
	float WorldGravity;

	FVector VelocityThen;
	FVector VelocityNow;
	FRotator RotationNow;

	FVector LinearAccelerationVector;
	FVector AngularVelocityVector;

	// Used for noise
	MultivariateNormal<3> mvnAccel;
	MultivariateNormal<3> mvnOmega;
	MultivariateNormal<3> mvnBiasAccel;
	MultivariateNormal<3> mvnBiasOmega;
	FVector BiasAccel = FVector(0);
	FVector BiasOmega = FVector(0);
};