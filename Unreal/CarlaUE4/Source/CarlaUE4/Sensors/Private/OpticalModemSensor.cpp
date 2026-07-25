// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "OpticalModemSensor.h"
#include "Json.h"

UOpticalModemSensor::UOpticalModemSensor() {
    PrimaryComponentTick.bCanEverTick = true;
	SensorName = "OpticalModemSensor";
}

void UOpticalModemSensor::InitializeSensor() {
	Super::InitializeSensor();
	//You need to get the pointer to the object the sensor is attached to. 
	Parent = this->GetAttachmentRootActor();
    NoiseMaxDistance = MaxDistance + DistanceNoise.sampleFloat();
    NoiseLaserAngle = LaserAngle + AngleNoise.sampleFloat();
}

void UOpticalModemSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	//check if your parent pointer is valid, and if the sensor is on. Then get the buffer before sending the data to it. 
    bool* BoolBuffer = static_cast<bool*>(Buffer);
    BoolBuffer[0] = false;

    if (Parent != nullptr && bOn) {
        NoiseMaxDistance = MaxDistance + DistanceNoise.sampleFloat();
        NoiseLaserAngle = LaserAngle + AngleNoise.sampleFloat();
		if (FromSensor) {
            // if someone starting transmitting and we're in range, we'll receive
            BoolBuffer[0] = this->CanTransmit();       

            // Reset FromSensor now that the message has been sent
            FromSensor = NULL;  
		}
    }
    

    if (LaserDebug) {
        DrawDebugCone(GetWorld(), GetComponentLocation(), GetForwardVector(), NoiseMaxDistance * 100, FMath::DegreesToRadians(NoiseLaserAngle), FMath::DegreesToRadians(NoiseLaserAngle), DebugNumSides, DebugColor, false, .01, ECC_WorldStatic, 1.F);
        // DrawDebugLine(GetWorld(), GetComponentLocation(), GetForwardVector() * NoiseMaxDistance * 100, DebugColor, false, .01,ECC_WorldStatic, 1.F);
    }
}

bool UOpticalModemSensor::CanTransmit() {    

    // get coordinates of other sensor in local frame
    FVector SendingSensor = this->GetComponentLocation();
    FVector ReceiveSensor = FromSensor->GetComponentLocation();
    FVector SendToReceive = ReceiveSensor - SendingSensor;
    FVector ReceiveToSend = SendingSensor - ReceiveSensor;

    float Dist = SendToReceive.Size() / 100;
    UE_LOG(LogHolodeck, Log, TEXT("Optical Modem: Dist = %f  MaxDistance = %f"), Dist, FromSensor->NoiseMaxDistance);

    bool transmit;

    //Max guaranteed range of modem is 50 meters
    if (Dist <= FromSensor->NoiseMaxDistance) {
    
        // Calculate if sensors are facing each other within 120 degrees
        //--> Difference in angle needs to be -60 < x < 60 
        //--> Check both sensors to make sure both are in acceptable orientations

        if (IsSensorOriented(this, SendToReceive) && IsSensorOriented(FromSensor, ReceiveToSend)) {        
            // Calculate if rangefinder and dist are equal or not.

            FCollisionQueryParams QueryParams = FCollisionQueryParams();
            QueryParams.AddIgnoredActor(Parent);

            FHitResult Hit = FHitResult();

            bool TraceResult = GetWorld()->LineTraceSingleByChannel(Hit, SendingSensor, ReceiveSensor, ECollisionChannel::ECC_Visibility, QueryParams);
           
            bool Range = (TraceResult && Hit.GetActor() == FromSensor->Parent);
                        
            if (Range) {
                // return true;
                transmit = true;
                UE_LOG(LogHolodeck, Log, TEXT("Optical Modem: Transmit success"));

            }
            else {
                transmit = false;
                UE_LOG(LogHolodeck, Log, TEXT("Optical Modem: Transmit failed due to range"));
                UE_LOG(LogHolodeck, Log, TEXT("Optical Modem: Range = %f"), Hit.Distance);

            }
        }
        else{
            transmit = false;
            UE_LOG(LogHolodeck, Log, TEXT("Optical Modem: Transmit failed due to orientation"));
        }
    }
    else{
        transmit = false;
        UE_LOG(LogHolodeck, Log, TEXT("Optical Modem: Transmit failed due to distance"));

    }
    return transmit;
    //return false;
}


bool UOpticalModemSensor::IsSensorOriented(UOpticalModemSensor* Sensor, FVector LocalToSensor) {
    float DotProduct = UKismetMathLibrary::Dot_VectorVector(UKismetMathLibrary::Normal(Sensor->GetForwardVector()), UKismetMathLibrary::Normal(LocalToSensor));
    float Angle = FMath::RadiansToDegrees(UKismetMathLibrary::Acos(DotProduct));
    UE_LOG(LogHolodeck, Log, TEXT("Optical Modem: angle = %f"), Angle);

    if (-1 * NoiseLaserAngle < Angle && Angle < NoiseLaserAngle) {
        return true;
    }
    else {
        return false;
    }
}

void UOpticalModemSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("MaxDistance")) {
			MaxDistance = JsonParsed->GetIntegerField("MaxDistance");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("LaserAngle")) {
			LaserAngle = JsonParsed->GetIntegerField("LaserAngle");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("DebugNumSides")) {
			DebugNumSides = JsonParsed->GetIntegerField("DebugNumSides");
		}

		if (JsonParsed->HasTypedField<EJson::Boolean>("LaserDebug")) {
			LaserDebug = JsonParsed->GetBoolField("LaserDebug");
		}
        if (JsonParsed->HasTypedField<EJson::String>("DebugColor")) {
            FillColorMap();
			DebugColor = ColorMap[JsonParsed->GetStringField("DebugColor")];
		}
        if (JsonParsed->HasTypedField<EJson::Number>("DistanceSigma")) {
			DistanceNoise.initSigma(JsonParsed->GetNumberField("DistanceSigma"));
        }
        if (JsonParsed->HasTypedField<EJson::Number>("AngleSigma")) {
			AngleNoise.initSigma(JsonParsed->GetNumberField("AngleSigma"));
        }
		if (JsonParsed->HasTypedField<EJson::Number>("DistanceCov")) {
			DistanceNoise.initCov(JsonParsed->GetNumberField("DistanceCov"));
		}
        if (JsonParsed->HasTypedField<EJson::Number>("AngleCov")) {
			AngleNoise.initCov(JsonParsed->GetNumberField("AngleCov"));
        }
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("URangeFinderSensor::ParseSensorParms:: Unable to parse json."));
	}
}

void UOpticalModemSensor::FillColorMap() {
    ColorMap.Add("Black", FColor::Black);
    ColorMap.Add("Blue", FColor::Blue);
    ColorMap.Add("Cyan", FColor::Cyan);
    ColorMap.Add("Emerald", FColor::Emerald);
    ColorMap.Add("Green", FColor::Green);
    ColorMap.Add("Magenta", FColor::Magenta);
    ColorMap.Add("Orange", FColor::Orange);
    ColorMap.Add("Purple", FColor::Purple);
    ColorMap.Add("Red", FColor::Red);
    ColorMap.Add("Silver", FColor::Silver);
    ColorMap.Add("Transparent", FColor::Transparent);
    ColorMap.Add("Turquoise", FColor::Turquoise);
    ColorMap.Add("White", FColor::White);
    ColorMap.Add("Yellow", FColor::Yellow);
    ColorMap.Add("Random", FColor::MakeRandomColor());
}