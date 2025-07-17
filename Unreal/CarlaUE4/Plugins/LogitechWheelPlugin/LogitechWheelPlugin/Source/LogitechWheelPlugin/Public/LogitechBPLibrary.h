// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LogitechSteeringWheelLib.h"
#include "ILogitechWheelPlugin.h"
#include "LogitechBPLibrary.generated.h"

/* 
*	Function library class.
*	Each function in it is expected to be static and represents blueprint node that can be called in any blueprint.
*
*	When declaring function you can define metadata for the node. Key function specifiers will be BlueprintPure and BlueprintCallable.
*	BlueprintPure - means the function does not affect the owning object in any way and thus creates a node without Exec pins.
*	BlueprintCallable - makes a function which can be executed in Blueprints - Thus it has Exec pins.
*	DisplayName - full name of the node, shown when you mouse over the node and in the blueprint drop down menu.
*				Its lets you name the node using characters not allowed in C++ function names.
*	CompactNodeTitle - the word(s) that appear on the node.
*	Keywords -	the list of keywords that helps you to find node when you search for it using Blueprint drop-down menu. 
*				Good example is "Print String" node which you can find also by using keyword "log".
*	Category -	the category your node will be under in the Blueprint drop-down menu.
*
*	For more info on custom blueprint nodes visit documentation:
*	https://wiki.unrealengine.com/Custom_Blueprint_Node_Creation
*/




UCLASS()
class ULogitechBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//The Wheel will automatically try to initialize
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Initialize Wheel", Keywords = "LogitechBPLibrary wheel steering initialize"), Category = "LogitechBPLibrary")
	static bool WheelInit(const bool ignoreXInputControllers);

	//The Wheel State is automatically updated every frame in LogitechWheelInputDevice.cpp
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Update Wheel State", Keywords = "LogitechBPLibrary wheel steering update state"), Category = "LogitechBPLibrary")
	static bool WheelUpdate();

	//Returns whether a wheel is connected at the provided index. index 0 = first controller
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Wheel Connected", Keywords = "LogitechBPLibrary wheel steering connected"), Category = "LogitechBPLibrary")
	static bool WheelIsConnected(const int index);

	/*Returns whether a logitech device is connected at the provided index. index 0 = first controller. 
	Device Types
		0 = Wheel
		1 = Joystick
		2 = Gamepad
		3 = Other
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Logitech Device Connected", Keywords = "LogitechBPLibrary wheel steering connected"), Category = "LogitechBPLibrary")
	static bool WheelIsDeviceConnected(const int index, const int deviceType);

	/*Checks if the device connected at index is made from the manufacturer specified by manufacturerName
	Manufacturer Names
		0 = Logitech
		1 = Microsoft
		2 = Other
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Wheel Manufacturer Connected", Keywords = "LogitechBPLibrary wheel steering connected"), Category = "LogitechBPLibrary")
	static bool WheelIsManufacturerConnected(const int index, const int manufacturerName);

	/* Checks if the device connected at index is the model specified.
	Model Names
		0 = G27					5 = Driving Force Pro		10 = Force 3D Pro		15 = Strice Force 3D	20 = Rumblepad 2			25 = Chillstream
		1 = Driving Force GT	6 = Driving Force			11 = Extreme 3D Pro		16 = G940 Joystick		21 = Cordless Rumblepad 2	26 = G29
		2 = G25					7 = Nascar Racing Wheel		12 = Freedom 24			17 = G940 Throttle		22 = Cordless Gamepad		27 = G920
		3 = Momo Racing			8 = Formula Force			13 = Attack 3			18 = G940 Pedals		23 = Dual Action Gamepad	
		4 = Momo Force			9 = Formula Force GP		14 = Force 3D			19 = Rumblepad			24 = Precision Gamepad 2
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Wheel Model Connected", Keywords = "LogitechBPLibrary wheel steering connected model"), Category = "LogitechBPLibrary")
	static bool WheelIsModelConnected(const int index, const int modelName);

	//Checks if the device connected at index is currently triggering the button specified
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Wheel Button Triggered", Keywords = "LogitechBPLibrary wheel steering button triggered"), Category = "LogitechBPLibrary")
	static bool WheelButtonTriggered(const int index, const int buttonNbr);

	//Checks if on the device connected at index has been released the button specified
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Wheel Button Released", Keywords = "LogitechBPLibrary wheel steering button released"), Category = "LogitechBPLibrary")
	static bool WheelButtonReleased(const int index, const int buttonNbr);

	//Checks if on the device connected at index is currently being pressed the button specified
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is Wheel Button Pressed", Keywords = "LogitechBPLibrary wheel steering button pressed"), Category = "LogitechBPLibrary")
	static bool WheelButtonIsPressed(const int index, const int buttonNbr);
	
	//The WheelGenerateNonLinearValues () function generate non-linear values for the game controller's axis.
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Wheel Generate Non Linear Values", Keywords = "LogitechBPLibrary wheel steering"), Category = "LogitechBPLibrary")
	static bool WheelGenerateNonLinearValues(const int index, const int nonLinCoeff);

	//The WheelGetNonLinearValue () function returns a non-linear value from a table previously generated. This can be  used for the response of a steering wheel.
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Wheel Get Non Linear Values", Keywords = "LogitechBPLibrary wheel steering"), Category = "LogitechBPLibrary")
	static int WheelGetNonLinearValue(const int index, const int inputValue);

	//Checks if the controller at index has force feedback
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Wheel Has Force Feedback", Keywords = "LogitechBPLibrary wheel steering force feedback"), Category = "LogitechBPLibrary")
	static bool WheelHasForceFeedback(const int index);

	/*Checks if a certain force effect is currently playing. 

	True if the LogiIsPlaying device can do force feedback, false otherwise.

	Force Types
		0 = Spring				5 = Dirt Road
		1 = Constant			6 = Bumpy Road
		2 = Damper				7 = Slippery Road
		3 = Side Collision		8 = Surface Effect
		4 = Frontal Collision	9 = Car Airborne
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Wheel Is Playing", Keywords = "LogitechBPLibrary wheel steering force feedback playing"), Category = "LogitechBPLibrary")
	static bool WheelIsPlaying(const int index, const int forceType);

	/*Plays the spring force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	offsetPercentage: Specifies the center of the spring force effect. Valid range is -100 to 100. Specifying 0 centers the spring. Any values outside this range are silently clamped.

	saturationPercentage: Specify the level of saturation of the spring force effect. The saturation stays constant after a certain deflection from the center of the spring. 
	It is comparable to a magnitude.  Valid ranges are 0 to 100. Any value higher than 100 is silently clamped. 

	coefficientPercentage - Specify the slope of the effect strength increase relative to the amount of deflection from the center of the condition.  
	Higher values mean that the saturation level is  reached sooner.  Valid ranges are -100 to 100. Any value outside the valid range is silently clamped.
	
	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Spring Force", Keywords = "LogitechBPLibrary wheel steering force feedback spring"), Category = "LogitechBPLibrary")
	static bool WheelPlaySpringForce(const int index, const int offsetPercentage, const int saturationPercentage, const int coefficientPercentage);

	/*Stops the spring force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Spring Force", Keywords = "LogitechBPLibrary wheel steering force feedback spring"), Category = "LogitechBPLibrary")
	static bool WheelStopSpringForce(const int index);

	/*Plays the constant force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	magnitudePercentage : Specifies the magnitude of the constant force effect. A negative value reverses the direction of the force. 
	Valid ranges for magnitudePercentage are -100 to 100. Any values outside the valid range are silently clamped.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Constant Force", Keywords = "LogitechBPLibrary wheel steering force feedback constant"), Category = "LogitechBPLibrary")
	static bool WheelPlayConstantForce(const int index, const int magnitudePercentage);

	/*Stops the constant force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Constant Force", Keywords = "LogitechBPLibrary wheel steering force feedback constant"), Category = "LogitechBPLibrary")
	static bool WheelStopConstantForce(const int index);

	/*Plays the damper force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	coefficientPercentage : specify the slope of the effect strength increase relative to the amount of deflection from the center of the condition.  
	Higher values mean that the saturation level is reached sooner.  Valid ranges are -100 to 100. Any value outside the valid range is silently clamped.
	-100 simulates a very slippery effect, +100 makes the wheel/joystick very hard to move, simulating the car at a stop or in mud.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Damper Force", Keywords = "LogitechBPLibrary wheel steering force feedback damper"), Category = "LogitechBPLibrary")
	static bool WheelPlayDamperForce(const int index, const int coefficientPercentage);

	/*Stops the damper force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Damper Force", Keywords = "LogitechBPLibrary wheel steering force feedback damper"), Category = "LogitechBPLibrary")
	static bool WheelStopDamperForce(const int index);

	/*Plays the side collision force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	magnitudePercentage : Specifies the magnitude of the side collision force effect. A negative value reverses the direction of the force. Valid ranges for magnitudePercentage are -100 to 100.
	Any values outside the valid range are silently clamped.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Side Collision Force", Keywords = "LogitechBPLibrary wheel steering force feedback side collision"), Category = "LogitechBPLibrary")
	static bool WheelPlaySideCollisionForce(const int index, const int magnitudePercentage);

	/*Plays the frontal collision force

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	magnitudePercentage : specifies the magnitude of the frontal collision force effect.  Valid ranges for magnitudePercentage are 0 to 100. Values higher than 100 are silently clamped.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Frontal Collision Force", Keywords = "LogitechBPLibrary wheel steering force feedback frontal collision"), Category = "LogitechBPLibrary")
	static bool WheelPlayFrontalCollisionForce(const int index, const int magnitudePercentage);

	/*Plays the dirt road effect. 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	magnitudePercentage : Specifies the magnitude of the dirt road effect.  Valid ranges for magnitudePercentage are 0 to 100. Values higher than 100 are silently clamped.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Dirt Road Effect", Keywords = "LogitechBPLibrary wheel steering force feedback dirt road"), Category = "LogitechBPLibrary")
	static bool WheelPlayDirtRoadEffect(const int index, const int magnitudePercentage);

	/*Stops the dirt road effect. 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Dirt Road Effect", Keywords = "LogitechBPLibrary wheel steering force feedback dirt road"), Category = "LogitechBPLibrary")
	static bool WheelStopDirtRoadEffect(const int index);

	/*Plays the bumpy road effect. 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	magnitudePercentage : Specifies the magnitude of the bumpy road effect.  Valid ranges for magnitudePercentage are 0 to 100. Values higher than 100 are silently clamped.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Bumpy Road Effect", Keywords = "LogitechBPLibrary wheel steering force feedback bumpy road"), Category = "LogitechBPLibrary")
	static bool WheelPlayBumpyRoadEffect(const int index, const int magnitudePercentage);

	/*Stops the bumpy road effect.

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Bumpy Road Effect", Keywords = "LogitechBPLibrary wheel steering force feedback bumpy road"), Category = "LogitechBPLibrary")
	static bool WheelStopBumpyRoadEffect(const int index);

	/*Plays the slippery road effect. 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	magnitudePercentage : Specifies the magnitude of the slippery road effect.  Valid ranges for magnitudePercentage are 0 to 100. 100 corresponds to the most slippery effect.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Slippery Road Effect", Keywords = "LogitechBPLibrary wheel steering force feedback slippery road"), Category = "LogitechBPLibrary")
	static bool WheelPlaySlipperyRoadEffect(const int index, const int magnitudePercentage);

	/*Stops the slippery road effect.

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Slippery Road Effect", Keywords = "LogitechBPLibrary wheel steering force feedback slippery road"), Category = "LogitechBPLibrary")
	static bool WheelStopSlipperyRoadEffect(const int index);

	/*Plays the surface effect.

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	type : Specifies the type of force effect. Can be one of the  following values:
		o LOGI_PERIODICTYPE_SINE
		o LOGI_PERIODICTYPE_SQUARE
		o LOGI_PERIODICTYPE_TRIANGLE

	magnitudePercentage - Specifies the magnitude of the surface effect.  Valid ranges for magnitudePercentage are 0 to 100. Values higher than 100 are silently clamped.

	period - Specifies the period of the periodic force effect. The value is the duration for one full cycle of the periodic function measured in milliseconds. 
	A good range of values for the period is  20 ms (sand) to 120 ms (wooden bridge or cobblestones). For a surface effect the period should not be any bigger than 150 ms.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Surface Effect", Keywords = "LogitechBPLibrary wheel steering force feedback surface"), Category = "LogitechBPLibrary")
	static bool WheelPlaySurfaceEffect(const int index, const int type, const int magnitudePercentage, const int period);
	
	/*Stops the surface effect. 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Surface Effect", Keywords = "LogitechBPLibrary wheel steering force feedback surface"), Category = "LogitechBPLibrary")
	static bool WheelStopSurfaceEffect(const int index);

	/*Plays the car airborne effect.

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Car Airborne Effect", Keywords = "LogitechBPLibrary wheel steering force feedback airborne"), Category = "LogitechBPLibrary")
	static bool WheelPlayCarAirborne(const int index);

	/*Stops the car airborne effect.

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Car Airborne Effect", Keywords = "LogitechBPLibrary wheel steering force feedback airborne"), Category = "LogitechBPLibrary")
	static bool WheelStopCarAirborne(const int index);

	/*Plays the soft stop force. 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	usableRangePercentage : specifies the deadband in percentage of the softstop force effect.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel Soft Stop Force", Keywords = "LogitechBPLibrary wheel steering force feedback soft stop"), Category = "LogitechBPLibrary")
	static bool WheelPlaySoftstopForce(const int index, const int usableRangePercentage);

	/*Stops the soft stop force.

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Stop Wheel Soft Stop Force", Keywords = "LogitechBPLibrary wheel steering force feedback soft stop"), Category = "LogitechBPLibrary")
	static bool WheelStopSoftstopForce(const int index);

	/*Gets current shifter mode (gated or sequential) 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	Return Value
		1 if shifter is gated,  0 if shifter is sequential,  -1 if unknown 
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Wheel Shifter Mode", Keywords = "LogitechBPLibrary wheel steering shift mode"), Category = "LogitechBPLibrary")
	static int WheelGetShifterMode(const int index);

	/*Sets the operating range of the controller with the range parameter 

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.
	
	range : the operating range to be set

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Wheel Operating Range", Keywords = "LogitechBPLibrary wheel steering operating range"), Category = "LogitechBPLibrary")
	static bool WheelSetOperatingRange(const int index, const int range);

	/*Fills the range parameter of the controller with the current controller operating range

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	range : integer to receive the current operating range 

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Wheel Operating Range", Keywords = "LogitechBPLibrary wheel steering operating range"), Category = "LogitechBPLibrary")
	static bool WheelGetOperatingRange(const int index, int& range);

	/*Plays the leds on the controller

	index : index of the game controller.  Index 0 corresponds to the first game controller connected. Index 1 to the second game controller.

	currentRPM  : current RPM

	rpmFirstLedTurnsOn : RPM when first LEDs are to turn on.

	rpmRedLine : just below this RPM, all LEDs will be on. Just above,  all LEDs will start flashing.

	True if success, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Wheel LEDS", Keywords = "LogitechBPLibrary wheel steering leds"), Category = "LogitechBPLibrary")
	static bool WheelPlayLeds(const int index, const float currentRPM, const float rpmFirstLedTurnsOn, const float rpmRedLine);

	//shuts down the SDK and destroys the controller objects 
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Shutdown Wheel", Keywords = "LogitechBPLibrary wheel steering shutdown"), Category = "LogitechBPLibrary")
	static void WheelShutdown();

	//Returns the state of the controller
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Wheel State", Keywords = "LogitechBPLibrary wheel steering state"), Category = "LogitechBPLibrary")
	static FDeviceState WheelGetState(const int index);

};
