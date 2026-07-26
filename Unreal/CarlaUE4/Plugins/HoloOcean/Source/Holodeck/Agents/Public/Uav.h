// MIT License (c) 2019 BYU PCCL see LICENSE file
#pragma once

#include "GameFramework/Pawn.h"
#include "HolodeckAgent.h"
#include "Uav.generated.h"

//All in radians. (rad/s, rad/s^2, etc.)
const float UAV_MASS = 3.856; //Kilograms
const float UAV_MU = 1;
const float UAV_MAX_ROLL = 6.5080;
const float UAV_MAX_PITCH = 5.087;
const float UAV_MAX_YAW_RATE = .8;
const float UAV_MAX_FORCE = 59.844;

UCLASS()
/**
* AUav
* Inherits from the HolodeckAgent class
* On any tick this object will:
*     Calculate the forces to apply using PID controllers, desired values, and current values.
*     Apply the given forces. 
* Desired values must be set by a controller. 
*/
class HOLODECK_API AUav : public AHolodeckAgent
{
	GENERATED_BODY()
public:
	AUav();
	virtual void InitializeAgent() override;
	virtual void Tick(float DeltaSeconds) override;
	void SubstepTick(float DeltaTime, FBodyInstance* BodyInstance);
	void UpdateForcesAndMoments(float DeltaTime);
	void ApplyForces();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Desired)
		float DesiredAltitude;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Desired)
		float DesiredYawRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Desired)
		float DesiredPitch;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Desired)
		float DesiredRoll;
	UPROPERTY(BlueprintReadWrite, Category = UAVMesh)
		UStaticMeshComponent* RootMesh;

	//See HolodeckAgent.h for descriptions of these overriden functions

	float GetRollTorqueToApply() { return -CommandArray[0]; };
	float GetPitchTorqueToApply() { return -CommandArray[1]; };
	float GetYawTorqueToApply() { return CommandArray[2]; };
	float GetThrustToApply() { return CommandArray[3]; };

	unsigned int GetRawActionSizeInBytes() const override { return 4 * sizeof(float); };
	void* GetRawActionBuffer() const override { return (void*)CommandArray; };

	// Allows agent to fall up to ~9 meters
	float GetAccelerationLimit() override { return 300; }

protected:
	//See HolodeckAgent.h for descriptions of these overriden functions

private:
	/**
	  * 0: RollTorqueToApply
	  * 1: PitchTorqueToApply
	  * 2: YawTorqueToApply
	  * 3: ThrustToApply
	  */
	float CommandArray[4];

	FCalculateCustomPhysics OnCalculateCustomPhysics;

	/**
	  * InitializePIDControllers
	  */
	void InitializePIDControllers();

};
