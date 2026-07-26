// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "Holodeck.h"

#include "GameFramework/Pawn.h"
#include "HolodeckAgentInterface.h"
#include "Conversion.h"
#include "HolodeckPawnController.h"
#include "HolodeckPawnControllerInterface.h"
#include "HolodeckGameInstance.h"
#include "HolodeckAgent.generated.h"


/* Forward declare Holodeck Sensor Class. */
class UHolodeckSensor;

/**
* AHolodeckAgent
* The base class for holodeck agents. 
* HolodeckAgents are controllable from python.
* A HolodeckAgent contains the logic for how it acts at a low level.
* To get to a higher level of control, the logic should be implemented in
* a HolodeckControlScheme.
*/
UCLASS()
class HOLODECK_API AHolodeckAgent : public AHolodeckAgentInterface {
	GENERATED_BODY()

public:
	/**
	  * Default Constructor
	  */
	AHolodeckAgent();

	/**
	  * BeginPlay
	  * Should only call Super() and InitializeAgent() all other initialization 
	  * code should be called from within that function
	  */
	virtual void BeginPlay() final;

	/**
	* InitializeAgent
	* All agent initialization code should go in here. 
	*/
	UFUNCTION(BlueprintCallable)
	virtual void InitializeAgent();

	/**
	  * Tick
	  * Ticks the agent.
	  * If it is overridden, it must be called by the child class!
	  * @param DeltaSeconds the time since the last tick.
	  */
	virtual void Tick(float DeltaSeconds) override;

	/**
	  * Teleport
	  * Instantly moves the agent to target location, with the orientation that was given
	  * If no orientation was given, orientation remains unchanged (see overloaded function)
	  * @param NewLocation The location to move to
	  * @param NewRotation The rotation that the object will take on
	  * @return Bool if the teleport was successful.
	  */
	bool Teleport(const FVector& NewLocation, const FRotator& NewRotation) override;

	/**
	  * Teleport
	  * Instantly moves the agent to target location, with the orientation that was given
	  * Orientation remains unchanged
	  * @param NewLocation The location to move to
	  * @return Bool if the teleport was successful.
	  */
	bool Teleport(const FVector& NewLocation) override;

	/**
	* SetState
	* Sets the state of the agent (pos,rot,vel,ang_vel)
	* @param NewLocation The location to move to
	* @param NewRotation The rotation that the object will take on
	* @param NewVelocity The new linear velocity
	* @param NewAngVelocity The new angular velocity
	* @return Bool if the teleport was successful.
	*/
	bool SetState(const FVector& NewLocation, const FRotator& NewRotation, const FVector& NewVelocity, const FVector& NewAngVelocity) override;

	/**
	* InitializeController
	* Hooks up everything with the controller. This is normally called in the beginPlay function,
	* but if you have to manually configure a controller, you will have to call this function after
	* you do it.
	*/
	bool InitializeController() override;

	/**
	  * GetRawActionSizeInBytes
	  * @return the number of bytes used by the action space.
	  */
	virtual unsigned int GetRawActionSizeInBytes() const override {
		check(0 && "You must override GetRawActionSizeInBytes");
		return 0;
	};

	/**
	  * GetRawActionBuffer
	  * @return a pointer to the start of the action buffer.
	  */
	virtual void* GetRawActionBuffer() const override {
		check(0 && "You must override GetRawActionBuffer");
		return nullptr;
	};

	/**
	* GetAccelerationLimit
	* @return Float The maximum acceleration the agent can endure before being abused. -1 means no limit.
	*/
	virtual float GetAccelerationLimit() { return -1; }

	UPROPERTY(BlueprintReadWrite)
	bool IsAbused;

	/* Stores pointers to all the sensors on the agent. */
	TMap<FString, UHolodeckSensor*> SensorMap;
	AHolodeckPawnControllerInterface* HolodeckController;

private:

	UHolodeckGameInstance* Instance;
	UHolodeckServer* Server;
};
