#pragma once

#include "Holodeck.h"

#include "Android.h"
#include "HolodeckPawnController.h"
#include "AndroidControlSchemeMaxTorque.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "JointMaxTorqueControlScheme.h"
#include "AndroidController.generated.h"

UCLASS()
class HOLODECK_API AAndroidController : public AHolodeckPawnController
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	AAndroidController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	* Default Destructor
	*/
	~AAndroidController();

	/**
	* OnPossess
	* Called when the controller possesses the pawn.
	* @param Pawn the pawn being possessed.
	*/
	void OnPossess(APawn* Pawn) override;

	void AddControlSchemes();

private:

	USkeletalMeshComponent* SkeletalMeshComponent;
	float* ActionBufferFloatPtr;
	AHolodeckAgent* ControlledAndroid;
	UJointMaxTorqueControlScheme* ControlScheme;

};
