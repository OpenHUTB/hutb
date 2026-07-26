// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckWorldSettings.h"

float AHolodeckWorldSettings::FixupDeltaSeconds(float DeltaSeconds, float RealDeltaSeconds) {
	return ConstantTimeDeltaBetweenTicks;
}

float AHolodeckWorldSettings::GetConstantTimeDeltaBetweenTicks() {
	return ConstantTimeDeltaBetweenTicks;
}

void AHolodeckWorldSettings::SetConstantTimeDeltaBetweenTicks(float Delta) {
	ConstantTimeDeltaBetweenTicks = Delta;
}
