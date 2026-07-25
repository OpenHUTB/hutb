// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CarlaUE4.h"

#include "HolodeckPawnController.h"
#include "SphereRobot.h"

#include "SphereRobotController.generated.h"

/**
 * A Holodeck Sphere Robot Controller
 */
UCLASS()
class CARLAUE4_API ASphereRobotController : public AHolodeckPawnController
{
	GENERATED_BODY()
	
public:
	/**
	  * Default Constructor
	  */
	ASphereRobotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	  * Default Destructor
	  */
	~ASphereRobotController();

	void AddControlSchemes() override {
		// No control schemes
	}
};
