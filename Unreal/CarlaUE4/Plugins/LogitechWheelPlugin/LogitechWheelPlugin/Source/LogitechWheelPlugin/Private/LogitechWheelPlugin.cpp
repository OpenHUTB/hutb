// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LogitechWheelPluginPrivatePCH.h"
#include "LogitechSteeringWheelLib.h"
#include "LogitechWheelInputDevice.h"

class FLogitechWheelPlugin : public ILogitechWheelPlugin
{
private:
	FLogitechWheelInputDevice* _device;

public:
	/** Implements the rest of the IInputDeviceModule interface **/

	/** Creates a new instance of the IInputDevice associated with this IInputDeviceModule **/
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);

	/** Called right after the module DLL has been loaded and the module object has been created **/
	virtual void StartupModule() override;

	/** Called before the module is unloaded, right before the module object is destroyed. **/
	virtual void ShutdownModule() override;

	virtual FLogitechWheelInputDevice GetDevice();
};

IMPLEMENT_MODULE(FLogitechWheelPlugin, LogitechWheelPlugin)

#define LOCTEXT_NAMESPACE "InputKeys"

const FKey ILogitechWheelPlugin::LogitechSW_Wheel("Wheel");
const FKey ILogitechWheelPlugin::LogitechSW_Accelerator("Accelerator");
const FKey ILogitechWheelPlugin::LogitechSW_Brake("Brake");
const FKey ILogitechWheelPlugin::LogitechSW_Clutch("Clutch");
const FKey ILogitechWheelPlugin::LogitechSW_POV1("POV1");
const FKey ILogitechWheelPlugin::LogitechSW_POV2("POV2");
const FKey ILogitechWheelPlugin::LogitechSW_POV3("POV3");
const FKey ILogitechWheelPlugin::LogitechSW_POV4("POV4");

const FKey ILogitechWheelPlugin::LogitechSW_WheelFaceButtonBottom("WheelFaceButtonBottom");
const FKey ILogitechWheelPlugin::LogitechSW_WheelFaceButtonRight("WheelFaceButtonRight");
const FKey ILogitechWheelPlugin::LogitechSW_WheelFaceButtonLeft("WheelFaceButtonLeft");
const FKey ILogitechWheelPlugin::LogitechSW_WheelFaceButtonTop("WheelFaceButtonTop");

const FKey ILogitechWheelPlugin::LogitechSW_WheelRightBumper("WheelRightBumper");
const FKey ILogitechWheelPlugin::LogitechSW_WheelLeftBumper("WheelLeftBumper");
const FKey ILogitechWheelPlugin::LogitechSW_WheelSpecialButtonRight("WheelSpecialButtonRight");
const FKey ILogitechWheelPlugin::LogitechSW_WheelSpecialButtonLeft("WheelSpecialButtonLeft");
const FKey ILogitechWheelPlugin::LogitechSW_WheelRightStickButton("WheelRightStickButton");
const FKey ILogitechWheelPlugin::LogitechSW_WheelLeftStickButton("WheelLeftStickButton");
const FKey ILogitechWheelPlugin::LogitechSW_WheelXboxButton("WheelXboxButton");

TSharedPtr<class IInputDevice> FLogitechWheelPlugin::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	UE_LOG(LogTemp, Warning, TEXT("Created new input device!"));

	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_Wheel, LOCTEXT("Wheel", "Logitech Wheel"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_Accelerator, LOCTEXT("Accelerator", "Logitech Accelerator Pedal"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_Brake, LOCTEXT("Brake", "Logitech Brake Pedal"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_Clutch, LOCTEXT("Clutch", "Logitech Clutch Pedal"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_POV1, LOCTEXT("POV1", "Logitech POV1"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_POV2, LOCTEXT("POV2", "Logitech POV2"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_POV3, LOCTEXT("POV3", "Logitech POV3"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_POV4, LOCTEXT("POV4", "Logitech POV4"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelFaceButtonBottom, LOCTEXT("WheelFaceButtonBottom", "Logitech Wheel Face Button Bottom"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelFaceButtonRight, LOCTEXT("WheelFaceButtonRight", "Logitech Wheel Face Button Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelFaceButtonLeft, LOCTEXT("WheelFaceButtonLeft", "Logitech Wheel Face Button Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelFaceButtonTop, LOCTEXT("WheelFaceButtonTop", "Logitech Wheel Face Button Top"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelRightBumper, LOCTEXT("WheelRightBumper", "Logitech Wheel Right Bumper"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelLeftBumper, LOCTEXT("WheelLeftBumper", "Logitech Wheel Left Bumper"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelSpecialButtonRight, LOCTEXT("WheelSpecialButtonRight", "Logitech Wheel Special Button Right"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelSpecialButtonLeft, LOCTEXT("WheelSpecialButtonLeft", "Logitech Wheel Special Button Left"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelRightStickButton, LOCTEXT("WheelRightStickButton", "Logitech Wheel Right Stick Button"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelLeftStickButton, LOCTEXT("WheelLeftStickButton", "Logitech Wheel Left Stick Button"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(ILogitechWheelPlugin::LogitechSW_WheelXboxButton, LOCTEXT("WheelXboxButton", "Logitech Wheel Xbox Button"), FKeyDetails::GamepadKey));

	// See LogitechWheelInputDevice.h for the definition of the IInputDevice we are returning here
	_device = new FLogitechWheelInputDevice(InMessageHandler);
	return MakeShareable(_device);
}

void FLogitechWheelPlugin::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	// Custom module-specific init can go here.

	UE_LOG(LogTemp, Warning, TEXT("LogitechWheelPlugin initiated!"));

	// IMPORTANT: This line registers our input device module with the engine.
	//	      If we do not register the input device module with the engine,
	//	      the engine won't know about our existence. Which means 
	//	      CreateInputDevice never gets called, which means the engine
	//	      will never try to poll for events from our custom input device.
	IModularFeatures::Get().RegisterModularFeature(IInputDeviceModule::GetModularFeatureName(), this);
}

void FLogitechWheelPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UE_LOG(LogTemp, Warning, TEXT("LogitechWheelPlugin shut down!"));
	// Unregister our input device module
	IModularFeatures::Get().UnregisterModularFeature(IInputDeviceModule::GetModularFeatureName(), this);
}

FLogitechWheelInputDevice FLogitechWheelPlugin::GetDevice()
{
	return *_device;
}

bool ILogitechWheelPlugin::WheelInit(const bool ignoreXInputControllers)
{
	return LogiSteeringInitialize(ignoreXInputControllers);
}
bool ILogitechWheelPlugin::WheelGetSdkVersion(int *majorNum, int *minorNum, int *buildNum)
{
	return LogiSteeringGetSdkVersion(majorNum, minorNum, buildNum);
}
bool ILogitechWheelPlugin::WheelUpdate()
{
	return LogiUpdate();
}
DIJOYSTATE2* ILogitechWheelPlugin::WheelGetState(const int index)
{
	return LogiGetState(index);
}
FString ILogitechWheelPlugin::WheelGetFriendlyProductName(const int index)
{
	int size = 0;
	wchar_t buffer[260];
	if (LogiGetFriendlyProductName(index, buffer, size))
	{
		return FString(size, buffer);
	}
	return FString();
}
bool ILogitechWheelPlugin::WheelIsConnected(const int index)
{
	return LogiIsConnected(index);
}
bool ILogitechWheelPlugin::WheelIsDeviceConnected(const int index, const int deviceType)
{
	return LogiIsDeviceConnected(index, deviceType);
}
bool ILogitechWheelPlugin::WheelIsManufacturerConnected(const int index, const int manufacturerName)
{
	return LogiIsManufacturerConnected(index, manufacturerName);
}
bool ILogitechWheelPlugin::WheelIsModelConnected(const int index, const int modelName)
{
	return LogiIsModelConnected(index, modelName);
}
bool ILogitechWheelPlugin::WheelButtonTriggered(const int index, const int buttonNbr)
{
	return LogiButtonTriggered(index, buttonNbr);
}
bool ILogitechWheelPlugin::WheelButtonReleased(const int index, const int buttonNbr)
{
	return LogiButtonReleased(index, buttonNbr);
}
bool ILogitechWheelPlugin::WheelButtonIsPressed(const int index, const int buttonNbr)
{
	return LogiButtonIsPressed(index, buttonNbr);
}
bool ILogitechWheelPlugin::WheelGenerateNonLinearValues(const int index, const int nonLinCoeff)
{
	return LogiGenerateNonLinearValues(index, nonLinCoeff);
}
int ILogitechWheelPlugin::WheelGetNonLinearValue(const int index, const int inputValue)
{
	return LogiGetNonLinearValue(index, inputValue);
}
bool ILogitechWheelPlugin::WheelHasForceFeedback(const int index)
{
	return LogiHasForceFeedback(index);
}
bool ILogitechWheelPlugin::WheelIsPlaying(const int index, const int forceType)
{
	return LogiIsPlaying(index, forceType);
}
bool ILogitechWheelPlugin::WheelPlaySpringForce(const int index, const int offsetPercentage, const int saturationPercentage, const int coefficientPercentage)
{
	return LogiPlaySpringForce(index, offsetPercentage, saturationPercentage, coefficientPercentage);
}
bool ILogitechWheelPlugin::WheelStopSpringForce(const int index)
{
	return LogiStopSpringForce(index);
}
bool ILogitechWheelPlugin::WheelPlayConstantForce(const int index, const int magnitudePercentage)
{
	return LogiPlayConstantForce(index, magnitudePercentage);
}
bool ILogitechWheelPlugin::WheelStopConstantForce(const int index)
{
	return LogiStopConstantForce(index);
}
bool ILogitechWheelPlugin::WheelPlayDamperForce(const int index, const int coefficientPercentage)
{
	return LogiPlayDamperForce(index, coefficientPercentage);
}
bool ILogitechWheelPlugin::WheelStopDamperForce(const int index)
{
	return LogiStopDamperForce(index);
}
bool ILogitechWheelPlugin::WheelPlaySideCollisionForce(const int index, const int magnitudePercentage)
{
	return LogiPlaySideCollisionForce(index, magnitudePercentage);
}
bool ILogitechWheelPlugin::WheelPlayFrontalCollisionForce(const int index, const int magnitudePercentage)
{
	return LogiPlayFrontalCollisionForce(index, magnitudePercentage);
}
bool ILogitechWheelPlugin::WheelPlayDirtRoadEffect(const int index, const int magnitudePercentage)
{
	return LogiPlayDirtRoadEffect(index, magnitudePercentage);
}
bool ILogitechWheelPlugin::WheelStopDirtRoadEffect(const int index)
{
	return LogiStopDirtRoadEffect(index);
}
bool ILogitechWheelPlugin::WheelPlayBumpyRoadEffect(const int index, const int magnitudePercentage)
{
	return LogiPlayBumpyRoadEffect(index, magnitudePercentage);
}
bool ILogitechWheelPlugin::WheelStopBumpyRoadEffect(const int index)
{
	return LogiStopBumpyRoadEffect(index);
}
bool ILogitechWheelPlugin::WheelPlaySlipperyRoadEffect(const int index, const int magnitudePercentage)
{
	return LogiPlaySlipperyRoadEffect(index, magnitudePercentage);
}
bool ILogitechWheelPlugin::WheelStopSlipperyRoadEffect(const int index)
{
	return LogiStopSlipperyRoadEffect(index);
}
bool ILogitechWheelPlugin::WheelPlaySurfaceEffect(const int index, const int type, const int magnitudePercentage, const int period)
{
	return LogiPlaySurfaceEffect(index, type, magnitudePercentage, period);
}
bool ILogitechWheelPlugin::WheelStopSurfaceEffect(const int index)
{
	return LogiStopSurfaceEffect(index);
}
bool ILogitechWheelPlugin::WheelPlayCarAirborne(const int index)
{
	return LogiPlayCarAirborne(index);
}
bool ILogitechWheelPlugin::WheelStopCarAirborne(const int index)
{
	return LogiStopCarAirborne(index);
}
bool ILogitechWheelPlugin::WheelPlaySoftstopForce(const int index, const int usableRangePercentage)
{
	return LogiPlaySoftstopForce(index, usableRangePercentage);
}
bool ILogitechWheelPlugin::WheelStopSoftstopForce(const int index)
{
	return LogiStopSoftstopForce(index);
}
bool ILogitechWheelPlugin::WheelSetPreferredControllerProperties(const ControllerPropertiesData properties)
{
	LogiControllerPropertiesData prop;
	memcpy(&prop, &properties, sizeof(LogiControllerPropertiesData));
	bool result = LogiSetPreferredControllerProperties(prop);
	return result;
}
bool ILogitechWheelPlugin::WheelGetCurrentControllerProperties(const int index, ControllerPropertiesData& properties)
{
	LogiControllerPropertiesData prop;
	memcpy(&prop, &properties, sizeof(LogiControllerPropertiesData));
	bool result = LogiGetCurrentControllerProperties(index, prop);
	if (result)
	{
		memcpy(&properties, &prop, sizeof(ControllerPropertiesData));
	}
	return result;
}
int ILogitechWheelPlugin::WheelGetShifterMode(const int index)
{
	return LogiGetShifterMode(index);
}
bool ILogitechWheelPlugin::WheelSetOperatingRange(const int index, const int range)
{
	return LogiSetOperatingRange(index, range);
}
bool ILogitechWheelPlugin::WheelGetOperatingRange(const int index, int& range)
{
	return LogiGetOperatingRange(index, range);
}
bool ILogitechWheelPlugin::WheelPlayLeds(const int index, const float currentRPM, const float rpmFirstLedTurnsOn, const float rpmRedLine)
{
	return LogiPlayLeds(index, currentRPM, rpmFirstLedTurnsOn, rpmRedLine);
}
void ILogitechWheelPlugin::WheelShutdown()
{
	return LogiSteeringShutdown();
}


#undef LOCTEXT_NAMESPACE
	