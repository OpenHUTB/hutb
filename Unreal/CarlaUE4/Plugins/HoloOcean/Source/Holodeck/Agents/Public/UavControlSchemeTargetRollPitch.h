#pragma once

#include "Holodeck.h"

#include "HolodeckPawnController.h"
#include "Uav.h"
#include "HolodeckControlScheme.h"
#include "SimplePID.h"
#include <math.h>

#include "UavControlSchemeTargetRollPitch.generated.h"

const float UAV_TAU_UP_ROLL = 0.1904;
const float UAV_TAU_UP_PITCH = 0.1904;
const float UAV_TAU_UP_YAW_RATE = 0.04; // 1644;
const float UAV_TAU_UP_FORCE = 0.1644;
const float UAV_TAU_DOWN_ROLL = 0.1904;
const float UAV_TAU_DOWN_PITCH = 0.1904;
const float UAV_TAU_DOWN_YAW_RATE = 0.04;// 0.2164;
const float UAV_TAU_DOWN_FORCE = 0.2164;
const float UAV_ROLL_P = 25.0;
const float UAV_ROLL_I = 0.0;
const float UAV_ROLL_D = 8.0;
const float UAV_PITCH_P = 25.0;
const float UAV_PITCH_I = 0.0;
const float UAV_PITCH_D = 8.0;
const float UAV_YAW_P = 20.0;
const float UAV_YAW_I = 0.0;
const float UAV_YAW_D = 5.0;
const float UAV_ALT_P = 305.0;
const float UAV_ALT_I = 100.0;
const float UAV_ALT_D = 600.0;

/**
* UUavControlSchemeTargetRollPitch
*/
UCLASS()
class HOLODECK_API UUavControlSchemeTargetRollPitch : public UHolodeckControlScheme {
public:
	GENERATED_BODY()

	UUavControlSchemeTargetRollPitch(const FObjectInitializer& ObjectInitializer);

	void Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) override;

	unsigned int GetControlSchemeSizeInBytes() const override {
		return 4 * sizeof(float);
	}

	void SetController(AHolodeckPawnController* const Controller) { UavController = Controller; };

private:
	FVector RotatorToEulerInZYX(const FRotator& Rotator) const;

	AHolodeckPawnController* UavController;
	AUav* Uav;

	// PID Controllers
	SimplePID RollController;
	SimplePID PitchController;
	SimplePID YawController;
	SimplePID AltitudeController;


};
