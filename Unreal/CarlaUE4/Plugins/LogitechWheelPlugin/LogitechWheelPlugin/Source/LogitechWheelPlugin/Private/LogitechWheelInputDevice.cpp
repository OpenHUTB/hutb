// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LogitechWheelInputDevice.h"
#include "LogitechWheelPluginPrivatePCH.h"
#include "LogitechSteeringWheelLib.h"
#include "ILogitechWheelPlugin.h"
#include "GenericPlatform/IInputInterface.h"

#define InputDevice ILogitechWheelPlugin::Get()


FLogitechWheelInputDevice::FLogitechWheelInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) :
	MessageHandler(InMessageHandler)
{

	
	// Initiate your device here
	InputDevice.WheelInit(true);
	IsDeviceAvailable(0);

	_buttonKeys[0] = ILogitechWheelPlugin::LogitechSW_WheelFaceButtonBottom;
	_buttonKeys[1] = ILogitechWheelPlugin::LogitechSW_WheelFaceButtonRight;
	_buttonKeys[2] = ILogitechWheelPlugin::LogitechSW_WheelFaceButtonLeft;
	_buttonKeys[3] = ILogitechWheelPlugin::LogitechSW_WheelFaceButtonTop;
	_buttonKeys[4] = ILogitechWheelPlugin::LogitechSW_WheelRightBumper;
	_buttonKeys[5] = ILogitechWheelPlugin::LogitechSW_WheelLeftBumper;
	_buttonKeys[6] = ILogitechWheelPlugin::LogitechSW_WheelSpecialButtonRight;
	_buttonKeys[7] = ILogitechWheelPlugin::LogitechSW_WheelSpecialButtonLeft;
	_buttonKeys[8] = ILogitechWheelPlugin::LogitechSW_WheelRightStickButton;
	_buttonKeys[9] = ILogitechWheelPlugin::LogitechSW_WheelLeftStickButton;
	_buttonKeys[10] = ILogitechWheelPlugin::LogitechSW_WheelXboxButton;

	_povKeys[0] = ILogitechWheelPlugin::LogitechSW_POV1;
	_povKeys[1] = ILogitechWheelPlugin::LogitechSW_POV2;
	_povKeys[2] = ILogitechWheelPlugin::LogitechSW_POV3;
	_povKeys[3] = ILogitechWheelPlugin::LogitechSW_POV4;

	_lastState.rgbButtons.Init(0, 128);
	_lastState.rglSlider.Init(0, 2);
	_lastState.rglVSlider.Init(0, 2);
	_lastState.rglASlider.Init(0, 2);
	_lastState.rglFSlider.Init(0, 2);
	_lastState.rgdwPOV.Init(0, 4);

	currentState.rgbButtons.Init(0, 128);
	currentState.rglSlider.Init(0, 2);
	currentState.rglVSlider.Init(0, 2);
	currentState.rglASlider.Init(0, 2);
	currentState.rglFSlider.Init(0, 2);
	currentState.rgdwPOV.Init(0, 4);
}


FLogitechWheelInputDevice::~FLogitechWheelInputDevice()
{
	// Close your device here
	InputDevice.WheelShutdown();
}


void FLogitechWheelInputDevice::Tick(float DeltaTime)
{
	// Nothing necessary to do (boilerplate code to complete the interface)
}


void FLogitechWheelInputDevice::SendControllerEvents()
{
	InputDevice.WheelUpdate();
	if (!IsDeviceAvailable(0)) {
		//UE_LOG(LogTemp, Warning, TEXT("Device Not Available"));
		return;
	}
	
	currentState = ConvertDeviceState(InputDevice.WheelGetState(0));

	// check if states has changed compared to previous state
	int stateChange = 0; //0 = no change, 1 = Button Pressed, 2 = Button Released;
	FKey currentButton;
	
	//Check every button if its state has changed
	for (int i = 0; i < 128; i++)
	{

		if (_lastState.rgbButtons[i] && !currentState.rgbButtons[i])
			stateChange = 2;
		else if (!_lastState.rgbButtons[i] && currentState.rgbButtons[i])
			stateChange = 1;
		else //if previous == current
			stateChange = 0;

		currentButton = _buttonKeys[i];

		//send button event, if applicable
		switch (stateChange)
		{
		case 1:
			UE_LOG(LogTemp, Warning, TEXT("SendLogitechWheelControllerEvents: button %d pressed."),i);
			SendButtonDownEvent(currentButton);
			break;
		case 2:
			UE_LOG(LogTemp, Warning, TEXT("SendLogitechWheelControllerEvents: button %d released."),i);
			SendButtonUpEvent(currentButton);
			break;
		default:
			break;
		}
	}


	//lX = Wheel Rotation. -32768 to 32767. -32768 = all the way to the left. 32767 = all the way to the right.
	//lY = Acceleration Pedal. -32768 to 32767. 32767 = pedal not pressed. -32768 = pedal fully pressed.
	//lRz = Brake Pedal. -32768 to 32767. Higher value = less pressure on brake pedal
	//rglSlider[0] = Clutch Pedal. -32768 to 32767. 32767 = pedal not pressed. -32768 = pedal fully pressed.
	//rgdwPOV[0] = Dpad. -1 = not pressed. 0 = Top. 0.25 = Right. 0.5 = Bottom. 0.75 = Left.


	//Axis Events										         (Value - Min) / (Max - Min)
	SendAxisEvent(ILogitechWheelPlugin::LogitechSW_Wheel, FMath::Clamp(float(currentState.lX), -32767.0f, 32767.0f) / 32767.0f);
	SendAxisEvent(ILogitechWheelPlugin::LogitechSW_Accelerator, fabs(((currentState.lY - 32767.0f) / (65535.0f))));
	SendAxisEvent(ILogitechWheelPlugin::LogitechSW_Brake, fabs(((currentState.lRz - 32767.0f) / (65535.0f))));
	SendAxisEvent(ILogitechWheelPlugin::LogitechSW_Clutch, fabs(((currentState.rglSlider[0] - 32767.0f) / (65535.0f))));
	
	for(int i = 0; i < 4; i++) {
		if (currentState.rgdwPOV[i] < 0) {
			SendAxisEvent(_povKeys[i], -1.0f);
		}
		else {
			SendAxisEvent(_povKeys[i], currentState.rgdwPOV[i] / 36000.0f);
		}
	}
	_lastState = currentState;	//save the state so we can compare it to the new state in the next frame

	//UE_LOG(LogTemp, Warning, TEXT("SendControllerEvents: Wheel %d."), currentState.lX);
	//UE_LOG(LogTemp, Warning, TEXT("SendControllerEvents: Accel Pedal %d."), currentState.lY);
	//UE_LOG(LogTemp, Warning, TEXT("SendControllerEvents: rglSlider[0] %d."), currentState.rglSlider[0]);
	//UE_LOG(LogTemp, Warning, TEXT("SendControllerEvents: rglSlider[1] %d."), currentState.rglSlider[1]);
	//UE_LOG(LogTemp, Warning, TEXT("SendControllerEvents: lFRz %d."), currentState.lFRz);
}


void FLogitechWheelInputDevice::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}


bool FLogitechWheelInputDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Nothing necessary to do (boilerplate code to complete the interface)
	return false;
}


void FLogitechWheelInputDevice::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	// Nothing necessary to do (boilerplate code to complete the interface)
}


void FLogitechWheelInputDevice::SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values)
{
	// Nothing necessary to do (boilerplate code to complete the interface)
}

void FLogitechWheelInputDevice::SendButtonUpEvent(FKey button)
{
	FKeyEvent keyEvent(button, FSlateApplication::Get().GetModifierKeys(), 0, 0, 0, 0);
	FSlateApplication::Get().ProcessKeyUpEvent(keyEvent);
}

void FLogitechWheelInputDevice::SendButtonDownEvent(FKey button)
{
	FKeyEvent keyEvent(button, FSlateApplication::Get().GetModifierKeys(), 0, 0, 0, 0);
	FSlateApplication::Get().ProcessKeyDownEvent(keyEvent);
}

void FLogitechWheelInputDevice::SendAxisEvent(FKey axis, float value)
{
	FAnalogInputEvent analogEvent(axis, FSlateApplication::Get().GetModifierKeys(), 0, false, 0, 0, value);
	FSlateApplication::Get().ProcessAnalogInputEvent(analogEvent);
}

bool FLogitechWheelInputDevice::IsDeviceAvailable(int index)
{
	int isConnected = InputDevice.WheelIsConnected(index);

	if (isConnected == 0) {
		//UE_LOG(LogTemp, Warning, TEXT("SendControllerEvents: controller is not connected."));
		return false;
	}
	
	else {
		//UE_LOG(LogTemp, Warning, TEXT("SendControllerEvents: controller is connected"));
		return true;
	}
	
}

FDeviceState FLogitechWheelInputDevice::ConvertDeviceState(DIJOYSTATE2* stateToConvert)
{
	FDeviceState newState;

	newState.rgbButtons.Init(0, 128);
	newState.rglSlider.Init(0, 2);
	newState.rglVSlider.Init(0, 2);
	newState.rglASlider.Init(0, 2);
	newState.rglFSlider.Init(0, 2);
	newState.rgdwPOV.Init(0, 4);

	newState.lX		= (uintptr_t)stateToConvert->lX;
	newState.lY		= (uintptr_t)stateToConvert->lY;
	newState.lZ		= (uintptr_t)stateToConvert->lZ;
	newState.lRx	= (uintptr_t)stateToConvert->lRx;
	newState.lRy	= (uintptr_t)stateToConvert->lRy;
	newState.lRz	= (uintptr_t)stateToConvert->lRz;
	newState.lVX	= (uintptr_t)stateToConvert->lVX;
	newState.lVY	= (uintptr_t)stateToConvert->lVY;
	newState.lVZ	= (uintptr_t)stateToConvert->lVZ;
	newState.lVRx	= (uintptr_t)stateToConvert->lVRx;
	newState.lVRy	= (uintptr_t)stateToConvert->lVRy;
	newState.lVRz	= (uintptr_t)stateToConvert->lVRz;
	newState.lAX	= (uintptr_t)stateToConvert->lAX;
	newState.lAY	= (uintptr_t)stateToConvert->lAY;
	newState.lAZ	= (uintptr_t)stateToConvert->lAZ;
	newState.lARx	= (uintptr_t)stateToConvert->lARx;
	newState.lARy	= (uintptr_t)stateToConvert->lARy;
	newState.lARz	= (uintptr_t)stateToConvert->lARz;
	newState.lFX	= (uintptr_t)stateToConvert->lFX;
	newState.lFY	= (uintptr_t)stateToConvert->lFY;
	newState.lFZ	= (uintptr_t)stateToConvert->lFZ;
	newState.lFRx	= (uintptr_t)stateToConvert->lFRx;
	newState.lFRy	= (uintptr_t)stateToConvert->lFRy;
	newState.lFRz	= (uintptr_t)stateToConvert->lFRz;

	for (int i = 0; i < 128;i++)
	{
		newState.rgbButtons[i] = (uintptr_t)stateToConvert->rgbButtons[i];
	}

	for (int i = 0; i < 2; i++)
	{
		newState.rglSlider[i] = (uintptr_t)stateToConvert->rglSlider[i];
		newState.rglVSlider[i] = (uintptr_t)stateToConvert->rglVSlider[i];
		newState.rglASlider[i] = (uintptr_t)stateToConvert->rglASlider[i];
		newState.rglFSlider[i] = (uintptr_t)stateToConvert->rglFSlider[i];
	}

	for (int i = 0; i < 4; i++)
	{
		newState.rgdwPOV[i] = (uintptr_t)stateToConvert->rgdwPOV[i];
	}

	return newState;
}

