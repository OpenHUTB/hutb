// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CoreMinimal.h"

#include "HandAgent.h" // TODO Delete This
#include "HolodeckPawnController.h"
#include "JointMaxTorqueControlScheme.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "HandAgentMaxTorqueFloat.h"

#include "HandAgentController.generated.h"

UCLASS()
class HOLODECK_API AHandAgentController : public AHolodeckPawnController
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	AHandAgentController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	* Default Destructor
	*/
	~AHandAgentController() {};

	/**
	* Possess
	* Called when the controller possesses the pawn.
	* @param Pawn the pawn being possessed.
	*/
	void OnPossess(APawn* Pawn) override;

	void AddControlSchemes();

private:

	USkeletalMeshComponent* SkeletalMeshComponent;
	float* ActionBufferFloatPtr;
	UJointMaxTorqueControlScheme* JointTorqueControlScheme;
	UHandAgentMaxTorqueFloat* HandAgentFloatControlScheme;
};
