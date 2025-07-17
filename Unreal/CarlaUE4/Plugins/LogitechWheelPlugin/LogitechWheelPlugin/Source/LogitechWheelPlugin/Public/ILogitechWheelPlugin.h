// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "LogitechWheelPluginPrivatePCH.h"
#include "LogitechSteeringWheelLib.h"
#include "IInputDeviceModule.h"
#include "ILogitechWheelPlugin.generated.h"		//must include the .generated file so we can use USTRUCT()


typedef struct ControllerPropertiesData
{
		bool forceEnable;
		int overallGain;
		int springGain;
		int damperGain;
		bool defaultSpringEnabled;
		int defaultSpringGain;
		bool combinePedals;
		int wheelRange;
		bool gameSettingsEnabled;
		bool allowGameSettings;


}ControllerPropertiesData;

USTRUCT(BlueprintType)
struct FDeviceState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lX;                     /* x-axis position						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lY;                     /* y-axis position						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lZ;                     /* z-axis position						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lRx;                    /* x-axis rotation						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lRy;                    /* y-axis rotation						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lRz;                    /* z-axis rotation						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		TArray<int32>		rglSlider;	            /* extra axes positions					*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		TArray<int32>		rgdwPOV;	            /* POV directions. POV1 = Wheel Dpad	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		TArray<int32>		rgbButtons;		        /* 128 buttons							*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lVX;                    /* x-axis velocity						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lVY;                    /* y-axis velocity						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lVZ;                    /* z-axis velocity						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lVRx;                   /* x-axis angular velocity				*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lVRy;                   /* y-axis angular velocity				*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lVRz;                   /* z-axis angular velocity				*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		TArray<int32>		rglVSlider;				/* extra axes velocities				*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lAX;                    /* x-axis acceleration					*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lAY;                    /* y-axis acceleration					*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lAZ;                    /* z-axis acceleration					*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lARx;                   /* x-axis angular acceleration			*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lARy;                   /* y-axis angular acceleration			*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lARz;                   /* z-axis angular acceleration			*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		TArray<int32>		rglASlider;		        /* extra axes accelerations				*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lFX;                    /* x-axis force							*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lFY;                    /* y-axis force							*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lFZ;                    /* z-axis force							*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lFRx;                   /* x-axis torque						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lFRy;                   /* y-axis torque						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		int32				lFRz;                   /* z-axis torque						*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logitech Wheel Plugin")
		TArray<int32>		rglFSlider;		        /* extra axes forces					*/

};


class ILogitechWheelPlugin : public IInputDeviceModule
{
public:
	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline ILogitechWheelPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< ILogitechWheelPlugin >("LogitechWheelPlugin");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("LogitechWheelPlugin");
	}


	static const FKey LogitechSW_Wheel;
	static const FKey LogitechSW_Accelerator;
	static const FKey LogitechSW_Brake;
	static const FKey LogitechSW_Clutch;
	static const FKey LogitechSW_POV1;
	static const FKey LogitechSW_POV2;
	static const FKey LogitechSW_POV3;
	static const FKey LogitechSW_POV4;

	static const FKey LogitechSW_WheelFaceButtonBottom;
	static const FKey LogitechSW_WheelFaceButtonRight;
	static const FKey LogitechSW_WheelFaceButtonLeft;
	static const FKey LogitechSW_WheelFaceButtonTop;

	static const FKey LogitechSW_WheelRightBumper;
	static const FKey LogitechSW_WheelLeftBumper;
	static const FKey LogitechSW_WheelSpecialButtonRight;
	static const FKey LogitechSW_WheelSpecialButtonLeft;
	static const FKey LogitechSW_WheelRightStickButton;
	static const FKey LogitechSW_WheelLeftStickButton;
	static const FKey LogitechSW_WheelXboxButton;

	

	bool WheelInit(const bool ignoreXInputControllers);
	bool WheelGetSdkVersion(int *majorNum, int *minorNum, int *buildNum);
	bool WheelUpdate();
	DIJOYSTATE2* WheelGetState(const int index);
	FString WheelGetFriendlyProductName(const int index);
	bool WheelIsConnected(const int index);
	bool WheelIsDeviceConnected(const int index, const int deviceType);
	bool WheelIsManufacturerConnected(const int index, const int manufacturerName);
	bool WheelIsModelConnected(const int index, const int modelName);
	bool WheelButtonTriggered(const int index, const int buttonNbr);
	bool WheelButtonReleased(const int index, const int buttonNbr);
	bool WheelButtonIsPressed(const int index, const int buttonNbr);
	bool WheelGenerateNonLinearValues(const int index, const int nonLinCoeff);
	int WheelGetNonLinearValue(const int index, const int inputValue);
	bool WheelHasForceFeedback(const int index);
	bool WheelIsPlaying(const int index, const int forceType);
	bool WheelPlaySpringForce(const int index, const int offsetPercentage, const int saturationPercentage, const int coefficientPercentage);
	bool WheelStopSpringForce(const int index);
	bool WheelPlayConstantForce(const int index, const int magnitudePercentage);
	bool WheelStopConstantForce(const int index);
	bool WheelPlayDamperForce(const int index, const int coefficientPercentage);
	bool WheelStopDamperForce(const int index);
	bool WheelPlaySideCollisionForce(const int index, const int magnitudePercentage);
	bool WheelPlayFrontalCollisionForce(const int index, const int magnitudePercentage);
	bool WheelPlayDirtRoadEffect(const int index, const int magnitudePercentage);
	bool WheelStopDirtRoadEffect(const int index);
	bool WheelPlayBumpyRoadEffect(const int index, const int magnitudePercentage);
	bool WheelStopBumpyRoadEffect(const int index);
	bool WheelPlaySlipperyRoadEffect(const int index, const int magnitudePercentage);
	bool WheelStopSlipperyRoadEffect(const int index);
	bool WheelPlaySurfaceEffect(const int index, const int type, const int magnitudePercentage, const int period);
	bool WheelStopSurfaceEffect(const int index);
	bool WheelPlayCarAirborne(const int index);
	bool WheelStopCarAirborne(const int index);
	bool WheelPlaySoftstopForce(const int index, const int usableRangePercentage);
	bool WheelStopSoftstopForce(const int index);
	bool WheelSetPreferredControllerProperties(const ControllerPropertiesData properties);
	bool WheelGetCurrentControllerProperties(const int index, ControllerPropertiesData& properties);
	int WheelGetShifterMode(const int index);
	bool WheelSetOperatingRange(const int index, const int range);
	bool WheelGetOperatingRange(const int index, int& range);
	bool WheelPlayLeds(const int index, const float currentRPM, const float rpmFirstLedTurnsOn, const float rpmRedLine);
	void WheelShutdown();

};