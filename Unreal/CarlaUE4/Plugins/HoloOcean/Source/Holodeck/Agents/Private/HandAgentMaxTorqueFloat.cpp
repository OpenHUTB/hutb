// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HandAgentMaxTorqueFloat.h"

void UHandAgentMaxTorqueFloat::Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) {

	float* InputCommandFloat = static_cast<float*>(InputCommand);

	// Call the other controller so it can do its thing
	this->TorqueControlScheme->Execute(CommandArray, InputCommand, DeltaSeconds);

	// Copy out the floating directions
	// Floating around is handled by the HandAgent itself
	for (int i = 0; i < 3; ++i) {
		static_cast<float*>(CommandArray)[AHandAgent::TOTAL_JOINT_DOF + i] = 
			InputCommandFloat[AHandAgent::TOTAL_JOINT_DOF + i];
	}

}

void UHandAgentMaxTorqueFloat::SetTorqueControlScheme(UJointMaxTorqueControlScheme* Scheme) {
	this->TorqueControlScheme = Scheme;
}

unsigned int UHandAgentMaxTorqueFloat::GetControlSchemeSizeInBytes() const {
	return this->TorqueControlScheme->GetControlSchemeSizeInBytes() + 3;
}