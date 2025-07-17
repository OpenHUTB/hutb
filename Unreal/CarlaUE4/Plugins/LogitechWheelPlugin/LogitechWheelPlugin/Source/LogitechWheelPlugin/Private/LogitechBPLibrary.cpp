// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LogitechBPLibrary.h"
#include "LogitechWheelPluginPrivatePCH.h"
#include "LogitechSteeringWheelLib.h"
#include "LogitechWheelInputDevice.h"

#define InputDevice ILogitechWheelPlugin::Get()

ULogitechBPLibrary::ULogitechBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

bool ULogitechBPLibrary::WheelInit(const bool ignoreXInputControllers)
{
	return InputDevice.WheelInit(ignoreXInputControllers);
}

bool ULogitechBPLibrary::WheelUpdate()
{
	return InputDevice.WheelUpdate();
}

bool ULogitechBPLibrary::WheelIsConnected(const int index)
{
	return InputDevice.WheelIsConnected(index);
}

bool ULogitechBPLibrary::WheelIsDeviceConnected(const int index, const int deviceType)
{
	return InputDevice.WheelIsDeviceConnected(index, deviceType);
}

bool ULogitechBPLibrary::WheelIsManufacturerConnected(const int index, const int manufacturerName)
{
	return InputDevice.WheelIsManufacturerConnected(index, manufacturerName);
}

bool ULogitechBPLibrary::WheelIsModelConnected(const int index, const int modelName)
{
	return InputDevice.WheelIsModelConnected(index, modelName);
}

bool ULogitechBPLibrary::WheelButtonTriggered(const int index, const int buttonNbr)
{
	return InputDevice.WheelButtonTriggered(index, buttonNbr);
}

bool ULogitechBPLibrary::WheelButtonReleased(const int index, const int buttonNbr)
{
	return InputDevice.WheelButtonReleased(index, buttonNbr);
}

bool ULogitechBPLibrary::WheelButtonIsPressed(const int index, const int buttonNbr)
{
	return InputDevice.WheelButtonIsPressed(index,buttonNbr);
}

bool ULogitechBPLibrary::WheelGenerateNonLinearValues(const int index, const int nonLinCoeff)
{
	return InputDevice.WheelGenerateNonLinearValues(index, nonLinCoeff);
}

int ULogitechBPLibrary::WheelGetNonLinearValue(const int index, const int inputValue)
{
	return InputDevice.WheelGetNonLinearValue(index, inputValue);
}

bool ULogitechBPLibrary::WheelHasForceFeedback(const int index)
{
	return InputDevice.WheelHasForceFeedback(index);
}

bool ULogitechBPLibrary::WheelIsPlaying(const int index, const int forceType)
{
	return InputDevice.WheelIsPlaying(index, forceType);
}

bool ULogitechBPLibrary::WheelPlaySpringForce(const int index, const int offsetPercentage, const int saturationPercentage, const int coefficientPercentage)
{
	return InputDevice.WheelPlaySpringForce(index, offsetPercentage, saturationPercentage, coefficientPercentage);
}

bool ULogitechBPLibrary::WheelStopSpringForce(const int index)
{
	return InputDevice.WheelStopSpringForce(index);
}

bool ULogitechBPLibrary::WheelPlayConstantForce(const int index, const int magnitudePercentage)
{
	return InputDevice.WheelPlayConstantForce(index, magnitudePercentage);
}

bool ULogitechBPLibrary::WheelStopConstantForce(const int index)
{
	return InputDevice.WheelStopConstantForce(index);
}

bool ULogitechBPLibrary::WheelPlayDamperForce(const int index, const int coefficientPercentage)
{
	return InputDevice.WheelPlayDamperForce(index, coefficientPercentage);
}

bool ULogitechBPLibrary::WheelStopDamperForce(const int index)
{
	return InputDevice.WheelStopDamperForce(index);
}

bool ULogitechBPLibrary::WheelPlaySideCollisionForce(const int index, const int magnitudePercentage)
{
	return InputDevice.WheelPlaySideCollisionForce(index,magnitudePercentage);
}

bool ULogitechBPLibrary::WheelPlayFrontalCollisionForce(const int index, const int magnitudePercentage)
{
	return InputDevice.WheelPlayFrontalCollisionForce(index, magnitudePercentage);
}
bool ULogitechBPLibrary::WheelPlayDirtRoadEffect(const int index, const int magnitudePercentage)
{
	return InputDevice.WheelPlayDirtRoadEffect(index,magnitudePercentage);
}
bool ULogitechBPLibrary::WheelStopDirtRoadEffect(const int index)
{
	return InputDevice.WheelStopDirtRoadEffect(index);
}
bool ULogitechBPLibrary::WheelPlayBumpyRoadEffect(const int index, const int magnitudePercentage)
{
	return InputDevice.WheelPlayBumpyRoadEffect(index, magnitudePercentage);
}
bool ULogitechBPLibrary::WheelStopBumpyRoadEffect(const int index)
{
	return InputDevice.WheelStopBumpyRoadEffect(index);
}
bool ULogitechBPLibrary::WheelPlaySlipperyRoadEffect(const int index, const int magnitudePercentage)
{
	return InputDevice.WheelPlaySlipperyRoadEffect(index,magnitudePercentage);
}
bool ULogitechBPLibrary::WheelStopSlipperyRoadEffect(const int index)
{
	return InputDevice.WheelStopSlipperyRoadEffect(index);
}
bool ULogitechBPLibrary::WheelPlaySurfaceEffect(const int index, const int type, const int magnitudePercentage, const int period)
{
	return InputDevice.WheelPlaySurfaceEffect(index,type,magnitudePercentage,period);
}
bool ULogitechBPLibrary::WheelStopSurfaceEffect(const int index)
{
	return InputDevice.WheelStopSurfaceEffect(index);
}
bool ULogitechBPLibrary::WheelPlayCarAirborne(const int index)
{
	return InputDevice.WheelPlayCarAirborne(index);
}
bool ULogitechBPLibrary::WheelStopCarAirborne(const int index)
{
	return InputDevice.WheelStopCarAirborne(index);
}
bool ULogitechBPLibrary::WheelPlaySoftstopForce(const int index, const int usableRangePercentage)
{
	return InputDevice.WheelPlaySoftstopForce(index,usableRangePercentage);
}
bool ULogitechBPLibrary::WheelStopSoftstopForce(const int index)
{
	return InputDevice.WheelStopSoftstopForce(index);
}
int ULogitechBPLibrary::WheelGetShifterMode(const int index)
{
	return InputDevice.WheelGetShifterMode(index);
}
bool ULogitechBPLibrary::WheelSetOperatingRange(const int index, const int range)
{
	return InputDevice.WheelSetOperatingRange(index, range);
}
bool ULogitechBPLibrary::WheelGetOperatingRange(const int index, int & range)
{
	return InputDevice.WheelGetOperatingRange(index, range);
}
bool ULogitechBPLibrary::WheelPlayLeds(const int index, const float currentRPM, const float rpmFirstLedTurnsOn, const float rpmRedLine)
{
	return InputDevice.WheelPlayLeds(index,currentRPM,rpmFirstLedTurnsOn,rpmRedLine);
}
void ULogitechBPLibrary::WheelShutdown()
{
	InputDevice.WheelShutdown();
}

FDeviceState ULogitechBPLibrary::WheelGetState(const int index)
{
	if (InputDevice.WheelIsConnected(index)) {
		return FLogitechWheelInputDevice::ConvertDeviceState(InputDevice.WheelGetState(index));
	}
	return FDeviceState();
}







