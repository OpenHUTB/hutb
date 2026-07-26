// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "Containers/Array.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Pawn.h"
#include "HolodeckBuoyantAgent.h"
#include "TorpedoAUV.generated.h"

const float TAUV_MIN_THRUST = -100;
const float TAUV_MAX_THRUST = 100;
const float TAUV_MIN_FIN = -45;
const float TAUV_MAX_FIN = 45;

UCLASS()
/**
* ATorpedoAUV
* Inherits from the HolodeckAgent class
* On any tick this object will apply the given forces.
* Desired values must be set by a controller.
*/
class HOLODECK_API ATorpedoAUV : public AHolodeckBuoyantAgent
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor.
	*/
	ATorpedoAUV();

	void InitializeAgent() override;

	/**
	* Tick
	* Called each frame.
	* @param DeltaSeconds the time since the last tick.
	*/
	void Tick(float DeltaSeconds) override;

	unsigned int GetRawActionSizeInBytes() const override { return 5 * sizeof(float); };
	void* GetRawActionBuffer() const override { return (void*)CommandArray; };

	// Allows agent to fall up to ~8 meters
	float GetAccelerationLimit() override { return 400; }

	// Location of all forces to apply
	FVector thruster = FVector(-120,0,0);
	TArray<FVector> finTranslation{ FVector(-105,7.07,0),
									FVector(-105,0,7.07),
									FVector(-105,-7.07,0),
									FVector(-105,0,-7.07) };
	TArray<FRotator> finRotation{ 	FRotator(0,0,0),
									FRotator(0,0,-90),
									FRotator(0,0,-180),
									FRotator(0,0,-270) };

	void ApplyFin(int i);

private:
	/** NOTE: These go counter-clockwise, starting in front right
	* 0: Left Fin
	* 1: Top Fin
	* 2: Right Fin
	* 3: Bottom Fin
	* 4: Thruster
	*/
	float CommandArray[5];

};
