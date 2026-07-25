// Written by joshgreaves.

#pragma once

#include "HolodeckAgent.h"
#include "HolodeckPawnControllerInterface.h"
#include "Components/SceneComponent.h"
#include "HolodeckSensor.generated.h"

/**
  * HolodeckSensor
  * Abstract base class for sensors within holodeck
  * Handles publishing sensor data
  * To function properly, HolodeckSensors must be attached to an agent that is controlled by 
  * a holodeck controller. 
  * To override, you must implement:
  *		GetDataKey
  *		GetDataLength
  *		TickSensorComponent
  * And in BeginPlay you must call
  *		Super::BeginPlay
  *	But you cannot override:
  *		TickComponent
  */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), abstract )
class HOLODECK_API UHolodeckSensor : public USceneComponent
{
	GENERATED_BODY()

public:	
	/**
	  * Default Constructor
	  */
	UHolodeckSensor();

	/**
	  * Contains all sensor initialization code and subscribes the sensor
	  * Subclasses should make sure to call Super::InitializeSensor()
	  */
	virtual void InitializeSensor();

	/**
	* SetAgentAndController 
	* Sets the agent and controller
	* Should be called before InitializeSensor()
	*/
	virtual void SetAgentAndController(AHolodeckPawnControllerInterface* ControllerParam, FString AgentName);

	/**
	* BeginPlay
	* Should only call Super::BeginPlay() and InitializeSensor() all other initialization
	*	code should be called from within that function
	*/
	virtual void BeginPlay() final;
	
	/**
	  * Publishes sensor data each tick
	  * Subclasses should NOT override this function.
	  * Instead, they should override TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
	  * TickSensorComponent is called from this
	  */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	/**
	* Override this function if sensor has parameters to initialize.
	*/
	virtual void ParseSensorParms(FString ParmsJson) {}

	virtual FString GetAgentName() { return this->AgentName; }

	FString AgentName;

	// Allows you to modify the sensor name in the editor to allow for duplicate sensors on an agent
	// Default SensorName should be set in child before calling Super::InitializeSensor 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SensorName = "";

	// Allows you to turn the sensor on and off in the editor
	UPROPERTY(EditAnywhere)
	bool bOn;

protected:

	/**
	  * Must be overridden by subclass
	  * Set the number of data items
	  * e.g. return 100
	  */
	virtual int GetNumItems() { check(0 && "You must override getNumItems"); return 0; };

	/**
	  * Must be overridden by subclass
	  * Set the size of each data item
	  * e.g. return sizeof(float)
	  */
	virtual int GetItemSize() { check(0 && "You must override getItemSize"); return 0; };

	/**
	  * TickSensorComponent
	  * Mimics Unreal Engine's UActorComponent::TickComponent function. See it's documentation for what the parameters are. 
	  * Must be overridden by subclass
	  * Handles the logic for ticking the sensor
	  * Make sure to set the data,
	  * Cast Buffer into the right kind of pointer and set the data.
	  */
	virtual void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) { 
		check(0 && "You must override TickSensorComponent"); };

	AHolodeckPawnControllerInterface* Controller;
	void* Buffer;

	const FString SensorDataKey = "_sensor_data";
};
