// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once

#include "Containers/Array.h"
#include "GameFramework/Pawn.h"
#include "HolodeckBuoyantAgent.h"
#include "HoveringAUV.generated.h"


UCLASS()
/**
* AHoveringAUV
* Inherits from the HolodeckAgent class
* On any tick this object will apply the given forces.
* Desired values must be set by a controller.
*/
class HOLODECK_API AHoveringAUV : public AHolodeckBuoyantAgent
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor.
	*/
	AHoveringAUV();

	void InitializeAgent() override;

	/**
	* Tick
	* Called each frame.
	* @param DeltaSeconds the time since the last tick.
	*/
	void Tick(float DeltaSeconds) override;

	unsigned int GetRawActionSizeInBytes() const override { return 8 * sizeof(float); };
	void* GetRawActionBuffer() const override { return (void*)CommandArray; };

	// Allows agent to fall up to ~8 meters
	float GetAccelerationLimit() override { return 400; }

	// Location of all thrusters
	TArray<FVector> thrusterLocations{ FVector(18.18, 22.14, -4), 
											FVector(18.18, -22.14, -4),
											FVector(-31.43, -22.14, -4),
											FVector(-31.43, 22.14, -4),
											FVector(7.39, 18.23, -0.21),
											FVector(7.39, -18.23, -0.21),
											FVector(-20.64, -18.23, -0.21),
											FVector(-20.64, 18.23, -0.21) };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BuoyancySettings)
		bool Perfect= true;

	void ApplyThrusters();

private:
	/** NOTE: These go counter-clockwise, starting in front right
	* 0: Vertical Front Starboard Thruster
	* 1: Vertical Front Port Thruster
	* 2: Vertical Back  Port Thruster
	* 3: Vertical Back  Starboard Thruster
	* 4: Angled   Front Starboard Thruster
	* 5: Angled   Front Port Thruster
	* 6: Angled   Back  Port Thruster
	* 7: Angled   Back  Starboard Thruster
	*/
	float CommandArray[8];

};
