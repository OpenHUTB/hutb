// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "GameFramework/WorldSettings.h"
#include "HolodeckWorldSettings.generated.h"

/**
  * AHolodeckWorldSettings
  * The global settings being used by the simulator.
  * This is where the tick time is set.
  */
UCLASS()
class AHolodeckWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	/**
	  * FixupDeltaSeconds
	  * Overidden to ensure a tick is 0.033 seconds.
	  */
	float FixupDeltaSeconds(float DeltaSeconds, float RealDeltaSeconds) override;

	AHolodeckWorldSettings() {
		int TicksPerSec;
		if (FParse::Value(FCommandLine::Get(), TEXT("TicksPerSec="), TicksPerSec)) {
			ConstantTimeDeltaBetweenTicks = 1.0 / TicksPerSec;
		}
	};

	/**
	  * GetConstantTimeDeltaBetweenTicks
	  * Gets the time between ticks.
	  * @return the time between ticks in seconds.
	  */
	float GetConstantTimeDeltaBetweenTicks();

	/**
	  * SetConstantTimeDeltaBetweenTicks(float Delta)
	  * Sets the time of one tick.
	  * @param Delta the time for one tick.
	  */
	void SetConstantTimeDeltaBetweenTicks(float Delta);

private:
	float ConstantTimeDeltaBetweenTicks = 0.033;
};
