// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LogitechWheelPluginPrivatePCH.h"
#include "LogitechSteeringWheelLib.h"
#include "LogitechWheelInputDevice.h"
#include "LogitechSdkSession.h"

namespace
{
FLogitechSdkSession WheelSession;
bool bIgnoreXInput = true;

bool InitializeWheelSdk()
{
	const bool bSuccess = LogiSteeringInitialize(bIgnoreXInput);
	UE_LOG(LogTemp, Log, TEXT("Logitech SDK initialize: %s"),
		bSuccess ? TEXT("success") : TEXT("failed; retry in 2 seconds"));
	return bSuccess;
}

void ShutdownWheelSdk()
{
	UE_LOG(LogTemp, Log, TEXT("Logitech SDK session cleanup"));
	LogiSteeringShutdown();
}
}

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
	WheelShutdown();
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
	bIgnoreXInput = ignoreXInputControllers;
	return WheelSession.Initialize(FPlatformTime::Seconds(), InitializeWheelSdk, ShutdownWheelSdk);
}
bool ILogitechWheelPlugin::WheelGetSdkVersion(int *majorNum, int *minorNum, int *buildNum)
{
	return LogiSteeringGetSdkVersion(majorNum, minorNum, buildNum);
}
bool ILogitechWheelPlugin::WheelUpdate()
{
	return WheelSession.Update(FPlatformTime::Seconds(), GFrameCounter,
		InitializeWheelSdk, []() { return LogiUpdate(); }, []() {
			for (int Index = 0; Index < LOGI_MAX_CONTROLLERS; ++Index)
				if (LogiIsConnected(Index) && LogiGetState(Index) != nullptr)
					return true;
			return false;
		}, ShutdownWheelSdk);
}
DIJOYSTATE2* ILogitechWheelPlugin::WheelGetState(const int index)
{
	return LogiGetState(index);
}
FString ILogitechWheelPlugin::WheelGetFriendlyProductName(const int index)
{
	const int size = 260;
	wchar_t buffer[size] = {};
	if (LogiGetFriendlyProductName(index, buffer, size))
	{
		return FString(buffer);
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
	WheelSession.Close(ShutdownWheelSdk);
}


#undef LOCTEXT_NAMESPACE

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWheelReconnectTest, "HUTB.Driving.WheelReconnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWheelReconnectTest::RunTest(const FString& Parameters)
{
	FLogitechSdkSession Session;
	int InitCalls = 0, PollCalls = 0, ShutdownCalls = 0;
	bool bInitWorks = false, bPollWorks = true, bHasDevice = true;
	auto Init = [&]() { ++InitCalls; return bInitWorks; };
	auto Poll = [&]() { ++PollCalls; return bPollWorks; };
	auto HasDevice = [&]() { return bHasDevice; };
	auto Shutdown = [&]() { ++ShutdownCalls; };
	auto Update = [&](double Now, uint64 Frame) {
		return Session.Update(Now, Frame, Init, Poll, HasDevice, Shutdown);
	};
	TestFalse(TEXT("First initialization fails"), Update(0.0, 0));
	TestEqual(TEXT("Failed init leaves the SDK untouched for retry"), ShutdownCalls, 0);
	TestEqual(TEXT("Do not poll an uninitialized SDK"), PollCalls, 0);
	TestFalse(TEXT("Retry is throttled"), Update(1.0, 1));
	TestEqual(TEXT("No premature initialization"), InitCalls, 1);
	bInitWorks = true;
	TestTrue(TEXT("Initialization retried successfully"), Update(2.0, 2));
	TestEqual(TEXT("Second initialization was really called"), InitCalls, 2);
	TestTrue(TEXT("Other consumers reuse initialization"), Session.Initialize(2.0, Init, Shutdown));
	TestEqual(TEXT("Healthy SDK not initialized twice"), InitCalls, 2);
	TestTrue(TEXT("Same-frame polling reuses sample"), Update(2.0, 2));
	TestEqual(TEXT("SDK polled once this frame"), PollCalls, 1);
	bPollWorks = false;
	TestFalse(TEXT("Read failure is reported"), Update(3.0, 3));
	bPollWorks = true;
	TestTrue(TEXT("Transient failure recovers without shutdown"), Update(3.5, 4));
	TestEqual(TEXT("No reset for a short interruption"), ShutdownCalls, 0);
	bPollWorks = false;
	Update(4.0, 5);
	TestFalse(TEXT("Persistent failure resets SDK"), Update(6.0, 6));
	TestEqual(TEXT("Stuck SDK shut down"), ShutdownCalls, 1);
	bPollWorks = true;
	TestTrue(TEXT("Reinitialized after persistent failure"), Update(6.1, 7));
	TestEqual(TEXT("Reconnect really initializes"), InitCalls, 3);
	bHasDevice = false;
	Update(7.0, 8);
	TestFalse(TEXT("Persistently empty device table also resets"), Update(9.0, 9));
	TestEqual(TEXT("Empty table caused cleanup"), ShutdownCalls, 2);
	bHasDevice = true;
	TestTrue(TEXT("Device usable after reconnect"), Update(9.1, 10));
	Session.Close(Shutdown);
	Session.Close(Shutdown);
	TestEqual(TEXT("Closing twice shuts down only once"), ShutdownCalls, 3);
	return true;
}
#endif
