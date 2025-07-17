// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/IInputInterface.h"
#include "LogitechSteeringWheelLib.h"
#include "IInputDevice.h"
//#include "LogitechWheelInputDevice.generated.h"




class FLogitechWheelInputDevice : public IInputDevice
{
public:
	FLogitechWheelInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	virtual ~FLogitechWheelInputDevice();

	/** Tick the interface (e.g. check for new controllers) */
	virtual void Tick(float DeltaTime) override;

	/** Poll for controller state and send events if needed */
	virtual void SendControllerEvents() override;

	/** Set which MessageHandler will get the events from SendControllerEvents. */
	virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override;

	/** Exec handler to allow console commands to be passed through for debugging */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** IForceFeedbackSystem pass through functions **/
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;

	//Check if the input device is available
	bool IsDeviceAvailable(int index);

	//Convert DIJOYSTATE2* to FDeviceState
	static FDeviceState ConvertDeviceState(DIJOYSTATE2* stateToConvert);
	
	//stores the last state of the input device
	FDeviceState _lastState;

	//stores the current state of  the input device
	FDeviceState currentState;

	FKey _buttonKeys[128];	
	FKey _povKeys[4];
	ControllerPropertiesData WheelProperties;

private:
	/* Message handler */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	void SendButtonUpEvent(FKey button);
	void SendButtonDownEvent(FKey button);
	void SendAxisEvent(FKey axis, float value);

};