// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "Holodeck.h"

#include "GameFramework/Pawn.h"

#include "HolodeckAgentInterface.generated.h"

/**
* AHolodeckAgentInterface
* The base class for holodeck agents.
* HolodeckAgents are controllable from python.
* A HolodeckAgent contains the logic for how it acts at a low level.
* To get to a higher level of control, the logic should be implemented in
* a HolodeckControlScheme.
*/
UCLASS()
class HOLODECK_API AHolodeckAgentInterface : public APawn {
	GENERATED_BODY()

public:
	/**
	* BeginPlay
	* Called when the game starts.
	*/
	virtual void BeginPlay() override { Super::BeginPlay(); };

	/**
	* Tick
	* Ticks the agent.
	* If it is overridden, it must be called by the child class!
	* @param DeltaSeconds the time since the last tick.
	*/
	virtual void Tick(float DeltaSeconds) override { Super::Tick(DeltaSeconds); };

	/**
	* SetState
	* Sets the state of the agent (pos, rot, vel, ang_vel)
	* @param NewLocation The location to move to
	* @param NewRotation The rotation that the object will take on
	* @param NewVelocity The new linear velocity
	* @param NewAngVelocity The new angular velocity
	* @return Bool if the teleport was successful.
	*/
	virtual bool SetState(const FVector& NewLocation, const FRotator& NewRotation, const FVector& NewVelocity, const FVector& NewAngVelocity){
		check(0 && "You must override SetState");
		return false;
	};

	/**
	* Teleport
	* Instantly moves the agent to target location, with the orientation that was given
	* If no orientation was given, orientation remains unchanged (see overloaded function)
	* @param NewLocation The location to move to
	* @param NewRotation The rotation that the object will take on
	* @return Bool if the teleport was successful.
	*/
	virtual bool Teleport(const FVector& NewLocation, const FRotator& NewRotation) {
		check(0 && "You must override Teleport");
		return false;
	};

	/**
	* Teleport
	* Instantly moves the agent to target location, with the orientation that was given
	* Orientation remains unchanged
	* @param NewLocation The location to move to
	* @return Bool if the teleport was successful.
	*/
	virtual bool Teleport(const FVector& NewLocation) {
		check(0 && "You must override Teleport");
		return false;
	};

	/**
	* InitializeController
	* Hooks up everything with the controller. This is normally called in the beginPlay function,
	* but if you have to manually configure a controller, you will have to call this function after
	* you do it.
	*/
	virtual bool InitializeController() {
		check(0 && "You must override InitializeController");
		return false;
	};

	/**
	* GetRawActionSizeInBytes
	* @return the number of bytes used by the action space.
	*/
	virtual unsigned int GetRawActionSizeInBytes() const {
		check(0 && "You must override GetRawActionSizeInBytes");
		return 0;
	};

	/**
	* GetRawActionBuffer
	* @return a pointer to the start of the action buffer.
	*/
	virtual void* GetRawActionBuffer() const {
		check(0 && "You must override GetRawActionBuffer");
		return nullptr;
	};

	// Must be set in the editor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString AgentName;

	UPROPERTY(BlueprintReadWrite)
		bool MainAgent;
};
