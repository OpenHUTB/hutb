// MIT License (c) 2021 BYU FRoStLab see LICENSE file
#pragma once
#include "Holodeck.h"
#include "HolodeckSensor.h"
#include "Kismet/KismetMathLibrary.h"
#include "MultivariateNormal.h"

#include "OpticalModemSensor.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HOLODECK_API UOpticalModemSensor : public UHolodeckSensor {
    GENERATED_BODY()

public:
    UOpticalModemSensor();

    virtual void InitializeSensor() override;
    UOpticalModemSensor* FromSensor = nullptr;

virtual void ParseSensorParms(FString ParmsJson) override;

protected:
	int GetNumItems() override { return 1; };

    int GetItemSize() override { return sizeof(bool); }

    void TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    //Max distance of the modem in meters
    UPROPERTY(EditAnywhere)
	float MaxDistance = 50;

    float NoiseMaxDistance;
    
    //Debug Variables
    UPROPERTY(EditAnywhere)
	float LaserAngle = 60;

    float NoiseLaserAngle;

	UPROPERTY(EditAnywhere)
	bool LaserDebug = false;

    UPROPERTY(EditAnywhere)
    int DebugNumSides = 72;  //Default so that each side is 5 degrees

    UPROPERTY(EditAnywhere)
    FColor DebugColor = FColor::Green;


private:

    AActor* Parent;
    bool IsSensorOriented(UOpticalModemSensor* Sensor, FVector LocalToSensor);
    bool CanTransmit();
    TMap<FString, FColor> ColorMap;
    void FillColorMap();
    MultivariateNormal<1> DistanceNoise;
    MultivariateNormal<1> AngleNoise;
};