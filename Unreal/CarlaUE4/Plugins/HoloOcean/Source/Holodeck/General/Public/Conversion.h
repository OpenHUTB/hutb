#pragma once

#include "Holodeck.h"
#include "Kismet/KismetMathLibrary.h"

const float UEUnitsPerMeter = 100.0;
const float UEUnitsPerMeterSquared = 10000;

enum ConvertType {UEToClient, ClientToUE, NoScale};

FRotator RPYToRotator(float Roll, float Pitch, float Yaw);

FVector RotatorToRPY(FRotator Rot);

FVector ConvertLinearVector(FVector Vector, ConvertType Type);

FVector ConvertAngularVector(FVector Vector, ConvertType Type);

FRotator ConvertAngularVector(FRotator Rotator, ConvertType);

FVector ConvertTorque(FVector Vector, ConvertType Type);

float ConvertClientDistanceToUnreal(float client);

float ConvertUnrealDistanceToClient(float unreal);