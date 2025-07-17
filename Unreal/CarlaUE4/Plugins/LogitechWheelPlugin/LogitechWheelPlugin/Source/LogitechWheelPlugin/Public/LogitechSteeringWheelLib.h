//
// Logitech Gaming Steering Wheel SDK
//
// Copyright (C) 2011-2015 Logitech. All rights reserved.
//

#pragma once

const int LOGI_MAX_CONTROLLERS = 2;

//Force types

const int LOGI_FORCE_NONE = -1;
const int LOGI_FORCE_SPRING = 0;
const int LOGI_FORCE_CONSTANT = 1;
const int LOGI_FORCE_DAMPER = 2;
const int LOGI_FORCE_SIDE_COLLISION = 3;
const int LOGI_FORCE_FRONTAL_COLLISION = 4;
const int LOGI_FORCE_DIRT_ROAD = 5;
const int LOGI_FORCE_BUMPY_ROAD = 6;
const int LOGI_FORCE_SLIPPERY_ROAD = 7;
const int LOGI_FORCE_SURFACE_EFFECT = 8;
const int LOGI_NUMBER_FORCE_EFFECTS = 9;
const int LOGI_FORCE_SOFTSTOP = 10;
const int LOGI_FORCE_CAR_AIRBORNE = 11;


//Periodic types  for surface effect

const int LOGI_PERIODICTYPE_NONE = -1;
const int LOGI_PERIODICTYPE_SINE = 0;
const int LOGI_PERIODICTYPE_SQUARE = 1;
const int LOGI_PERIODICTYPE_TRIANGLE = 2;


//Devices types

const int LOGI_DEVICE_TYPE_NONE = -1;
const int LOGI_DEVICE_TYPE_WHEEL = 0;
const int LOGI_DEVICE_TYPE_JOYSTICK = 1;
const int LOGI_DEVICE_TYPE_GAMEPAD = 2;
const int LOGI_DEVICE_TYPE_OTHER = 3;
const int LOGI_NUMBER_DEVICE_TYPES = 4;

//Manufacturer types

const int LOGI_MANUFACTURER_NONE = -1;
const int LOGI_MANUFACTURER_LOGITECH = 0;
const int LOGI_MANUFACTURER_MICROSOFT = 1;
const int LOGI_MANUFACTURER_OTHER = 2;


//Model types

const int LOGI_MODEL_G27 = 0;
const int LOGI_MODEL_DRIVING_FORCE_GT = 1;
const int LOGI_MODEL_G25 = 2;
const int LOGI_MODEL_MOMO_RACING = 3;
const int LOGI_MODEL_MOMO_FORCE = 4;
const int LOGI_MODEL_DRIVING_FORCE_PRO = 5;
const int LOGI_MODEL_DRIVING_FORCE = 6;
const int LOGI_MODEL_NASCAR_RACING_WHEEL = 7;
const int LOGI_MODEL_FORMULA_FORCE = 8;
const int LOGI_MODEL_FORMULA_FORCE_GP = 9;
const int LOGI_MODEL_FORCE_3D_PRO = 10;
const int LOGI_MODEL_EXTREME_3D_PRO = 11;
const int LOGI_MODEL_FREEDOM_24 = 12;
const int LOGI_MODEL_ATTACK_3 = 13;
const int LOGI_MODEL_FORCE_3D = 14;
const int LOGI_MODEL_STRIKE_FORCE_3D = 15;
const int LOGI_MODEL_G940_JOYSTICK = 16;
const int LOGI_MODEL_G940_THROTTLE = 17;
const int LOGI_MODEL_G940_PEDALS = 18;
const int LOGI_MODEL_RUMBLEPAD = 19;
const int LOGI_MODEL_RUMBLEPAD_2 = 20;
const int LOGI_MODEL_CORDLESS_RUMBLEPAD_2 = 21;
const int LOGI_MODEL_CORDLESS_GAMEPAD = 22;
const int LOGI_MODEL_DUAL_ACTION_GAMEPAD = 23;
const int LOGI_MODEL_PRECISION_GAMEPAD_2 = 24;
const int LOGI_MODEL_CHILLSTREAM = 25;
const int LOGI_MODEL_G29 = 26;
const int LOGI_MODEL_G920 = 27;
const int LOGI_NUMBER_MODELS = 28;

typedef struct DIJOYSTATE2 {
	int				lX;                     /* x-axis position              */
	int				lY;                     /* y-axis position              */
	int				lZ;                     /* z-axis position              */
	int				lRx;                    /* x-axis rotation              */
	int				lRy;                    /* y-axis rotation              */
	int				lRz;                    /* z-axis rotation              */
	int				rglSlider[2];           /* extra axes positions         */
	unsigned int	rgdwPOV[4];             /* POV directions               */
	unsigned char   rgbButtons[128];        /* 128 buttons                  */
	int				lVX;                    /* x-axis velocity              */
	int				lVY;                    /* y-axis velocity              */
	int				lVZ;                    /* z-axis velocity              */
	int				lVRx;                   /* x-axis angular velocity      */
	int				lVRy;                   /* y-axis angular velocity      */
	int				lVRz;                   /* z-axis angular velocity      */
	int				rglVSlider[2];          /* extra axes velocities        */
	int				lAX;                    /* x-axis acceleration          */
	int				lAY;                    /* y-axis acceleration          */
	int				lAZ;                    /* z-axis acceleration          */
	int				lARx;                   /* x-axis angular acceleration  */
	int				lARy;                   /* y-axis angular acceleration  */
	int				lARz;                   /* z-axis angular acceleration  */
	int				rglASlider[2];          /* extra axes accelerations     */
	int				lFX;                    /* x-axis force                 */
	int				lFY;                    /* y-axis force                 */
	int				lFZ;                    /* z-axis force                 */
	int				lFRx;                   /* x-axis torque                */
	int				lFRy;                   /* y-axis torque                */
	int				lFRz;                   /* z-axis torque                */
	int				rglFSlider[2];          /* extra axes forces            */
} DIJOYSTATE2;


typedef struct LogiControllerPropertiesData
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
}LogiControllerPropertiesData;


//Call this function before any other of the following
bool LogiSteeringInitialize(bool ignoreXInputControllers);

//Get the current SDK Version number
bool LogiSteeringGetSdkVersion(int *majorNum, int *minorNum, int *buildNum);

//Update the status of the controller
bool LogiUpdate();

//Get the state of the controller in the standard way.
DIJOYSTATE2* LogiGetState(const int index);

//Get the friendly name of the product at index
bool LogiGetFriendlyProductName(const int index, wchar_t *buffer, int size);

//Check if a generic device at index is connected
bool LogiIsConnected(const int index);

//Check if the device connected at index is of the same type specified by deviceType
bool LogiIsDeviceConnected(const int index, const int deviceType);

//Check if the device connected at index is made from the manufacturer specified by manufacturerName
bool LogiIsManufacturerConnected(const int index, const int manufacturerName);

//Check if the device connected at index is the model specified by modelName
bool LogiIsModelConnected(const int index, const int modelName);

//Check if the device connected at index is currently triggering the button specified by buttonNbr
bool LogiButtonTriggered(const int index, const int buttonNbr);

//Check if on the device connected at index has been released the button specified by buttonNbr
bool LogiButtonReleased(const int index, const int buttonNbr);

//Check if on the device connected at index is currently being pressed the button specified by buttonNbr 
bool LogiButtonIsPressed(const int index, const int buttonNbr);

//Generate non-linear values for the axis of the controller at index
bool LogiGenerateNonLinearValues(const int index, const int nonLinCoeff);

//Get a non-linear value from a table previously generated
int LogiGetNonLinearValue(const int index, const int inputValue);

//Check if the controller at index has force feedback
bool LogiHasForceFeedback(const int index);

//Check if the controller at index is playing the force specified by forceType
bool LogiIsPlaying(const int index, const int forceType);

//Play the spring force on the controller at index with the specified parameters
bool LogiPlaySpringForce(const int index, const int offsetPercentage, const int saturationPercentage, const int coefficientPercentage);

//Stop the spring force on the controller at index
bool LogiStopSpringForce(const int index);

//Play the constant force on the controller at index with the specified parameter
bool LogiPlayConstantForce(const int index, const int magnitudePercentage);

//Stop the constant force on the controller at index
bool LogiStopConstantForce(const int index);

//Play the damper force on the controller at index with the specified parameter
bool LogiPlayDamperForce(const int index, const int coefficientPercentage);

//Stop the damper force on the controller at index
bool LogiStopDamperForce(const int index);

//Play the side collision force on the controller at index with the specified parameter
bool LogiPlaySideCollisionForce(const int index, const int magnitudePercentage);

//Play the frontal collision force on the controller at index with the specified parameter
bool LogiPlayFrontalCollisionForce(const int index, const int magnitudePercentage);

//Play the dirt road effect on the controller at index with the specified parameter
bool LogiPlayDirtRoadEffect(const int index, const int magnitudePercentage);

//Stop the dirt road effect on the controller at index
bool LogiStopDirtRoadEffect(const int index);

//Play the bumpy road effect on the controller at index with the specified parameter
bool LogiPlayBumpyRoadEffect(const int index, const int magnitudePercentage);

//Stop the bumpy road effect on the controller at index
bool LogiStopBumpyRoadEffect(const int index);

//Play the slippery road effect on the controller at index with the specified parameter
bool LogiPlaySlipperyRoadEffect(const int index, const int magnitudePercentage);

//Stop the slippery road effect on the controller at index
bool LogiStopSlipperyRoadEffect(const int index);

//Play the surface effect on the controller at index with the specified parameter
bool LogiPlaySurfaceEffect(const int index, const int type, const int magnitudePercentage, const int period);

//Stop the surface effect on the controller at index
bool LogiStopSurfaceEffect(const int index);

//Play the car airborne effect on the controller at index
bool LogiPlayCarAirborne(const int index);

//Stop the car airborne effect on the controller at index
bool LogiStopCarAirborne(const int index);

//Play the soft stop force on the controller at index with the specified parameter
bool LogiPlaySoftstopForce(const int index, const int usableRangePercentage);

//Stop the soft stop force on the controller at index
bool LogiStopSoftstopForce(const int index);

//Set preferred wheel properties specified by the struct properties
bool LogiSetPreferredControllerProperties(const LogiControllerPropertiesData properties);

//Fills the properties parameter with the current controller properties
bool LogiGetCurrentControllerProperties(const int index, LogiControllerPropertiesData& properties);

//get current shifter mode (gated or sequential)
int LogiGetShifterMode(const int index);

//Sets the operating range in degrees on the controller at the index.
bool LogiSetOperatingRange(const int index, const int range);

//Gets the current operating range in degrees on the controller at the index.
bool LogiGetOperatingRange(const int index, int& range);

//Play the leds on the controller at index applying the specified parameters.
bool LogiPlayLeds(const int index, const float currentRPM, const float rpmFirstLedTurnsOn, const float rpmRedLine);

//Call this function to shutdown the SDK and destroy the controller and wheel objects
void LogiSteeringShutdown();