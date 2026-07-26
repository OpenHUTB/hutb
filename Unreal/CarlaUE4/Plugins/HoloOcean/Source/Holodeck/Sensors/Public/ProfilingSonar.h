// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once

#include "Holodeck.h"
#include "HolodeckSensor.h"

#include "ImagingSonar.h"

#include "ProfilingSonar.generated.h"

/**
 * UProfilingSonar
 */
UCLASS()
class HOLODECK_API UProfilingSonar : public UImagingSonar
{
	GENERATED_BODY()
	
public:
	/*
	* Default Constructor
	*/
	UProfilingSonar();

	/**
	* Allows parameters to be set dynamically
	*/
	virtual void ParseSensorParms(FString ParmsJson) override;

};
