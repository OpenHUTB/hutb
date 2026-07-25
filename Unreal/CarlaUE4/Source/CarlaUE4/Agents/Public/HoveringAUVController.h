// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once

#include "CarlaUE4.h"

#include "HolodeckPawnController.h"
#include "HoveringAUV.h"

#include "HoveringAUVController.generated.h"

/**
* A Holodeck Turtle Agent Controller
*/
UCLASS()
class CARLAUE4_API AHoveringAUVController : public AHolodeckPawnController
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	AHoveringAUVController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	* Default Destructor
	*/
	~AHoveringAUVController();

	void AddControlSchemes() override {
		// No control schemes
	}
};
