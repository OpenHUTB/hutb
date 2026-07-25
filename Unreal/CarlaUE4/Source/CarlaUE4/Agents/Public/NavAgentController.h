// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "Holodeck.h"
#include "HolodeckPawnController.h"
#include "NavAgent.h"

#include "NavAgentController.generated.h"

/**
* A Holodeck Nav Agent Controller
*/
UCLASS()
class HOLODECK_API ANavAgentController : public AHolodeckPawnController
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	ANavAgentController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	* Default Destructor
	*/
	~ANavAgentController();

	void AddControlSchemes() override {
		// No Extra Control Schemes
	}
};
