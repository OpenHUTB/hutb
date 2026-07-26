// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "HolodeckCore/Public/HolodeckControlScheme.h"
#include "JointMaxTorqueControlScheme.h"
#include "HandAgent.h"

#include "HandAgentMaxTorqueFloat.generated.h"

/**
 * UHandAgentMaxTorqueFloat - JointMaxTorqueControlScheme, but the last three elements in the
 * command array control the Hand agent's movement in the x, y, and z planes so it can float around.
 */
UCLASS()
class HOLODECK_API UHandAgentMaxTorqueFloat : public UHolodeckControlScheme
{
	GENERATED_BODY()

public:
	UHandAgentMaxTorqueFloat(const FObjectInitializer& ObjectInitializer) {};

	void Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) override;

	unsigned int GetControlSchemeSizeInBytes() const override;

	/**
	 * This control scheme delegates most of its work to this UJointMaxTorqueControlScheme,
	 * which must be provided with this method after initializing it.
	 */
	void SetTorqueControlScheme(UJointMaxTorqueControlScheme* Scheme);

private:
	UJointMaxTorqueControlScheme *TorqueControlScheme;

};
