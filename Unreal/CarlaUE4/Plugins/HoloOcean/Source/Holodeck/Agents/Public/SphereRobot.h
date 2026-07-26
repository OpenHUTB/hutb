// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "Holodeck.h"

#include "HolodeckAgent.h"

#include "SphereRobot.generated.h"

static float MAX_ROTATION_SPEED = 20;
static float MAX_FORWARD_SPEED = 20;

UCLASS()
class HOLODECK_API ASphereRobot : public AHolodeckAgent
{
	GENERATED_BODY()

public:
	/**
	  * Default Constructor.
	  */
	ASphereRobot();

	/**
	* BeginPlay
	* Called when the game starts.
	* Registers the reward and terminal signals.
	*/
	void InitializeAgent() override;
	
	/**
	  * Tick
	  * Called each frame.
	  * @param DeltaSeconds the time since the last tick.
	  */
	void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ForwardSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RotSpeed;

	unsigned int GetRawActionSizeInBytes() const override { return 2 * sizeof(float); };
	void* GetRawActionBuffer() const override { return (void*)CommandArray; };

private:
	float CommandArray[2];
};
