#pragma once

#include <algorithm>

#include "Holodeck.h"

#include "GameFramework/Pawn.h"
#include "HolodeckAgent.h"

#include "HandAgent.generated.h"

UCLASS(Blueprintable)
class HOLODECK_API AHandAgent : public AHolodeckAgent
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor
	*/
	AHandAgent();

	static constexpr int NUM_JOINTS = 16;
	static constexpr int NUM_3_AXIS_JOINTS = 1;
	static constexpr int NUM_2_AXIS_JOINTS = 5;
	static constexpr int NUM_1_AXIS_JOINTS = 10;
	static constexpr int NUM_2_PLUS_3_AXIS_JOINTS = NUM_3_AXIS_JOINTS + NUM_2_AXIS_JOINTS;
	static constexpr int NUM_FLOAT_DIRECTIONS = 3;
	static constexpr int TOTAL_JOINT_DOF = NUM_3_AXIS_JOINTS * 3 +
										   NUM_2_AXIS_JOINTS * 2 +
										   NUM_1_AXIS_JOINTS;

	static constexpr int TOTAL_DOF = TOTAL_JOINT_DOF + NUM_FLOAT_DIRECTIONS;
	static constexpr float MAX_MOVEMENT_METERS = 0.5;
	static constexpr float MAX_TORQUE = 30;

	const static FName Joints[];
	const static FName BoneNames[];
	const static int NumBones;

	/**
	 * Called when the game starts.
	 */
	virtual void InitializeAgent() override;

	/**
	 * Tick
	 * Called every frame.
	 * @param DeltaSeconds the time since the last tick.
	 */
	void Tick(float DeltaSeconds) override;


	/**
	 * GetJoints
	 * Gets pointer to constant array of FName joints
	 * @return array of FName corresponding to android joint names
	 */
	const FName* GetJoints();


	UPROPERTY(BlueprintReadWrite, Category = AndroidMesh)
		USkeletalMeshComponent* SkeletalMesh;

	unsigned int GetRawActionSizeInBytes() const override {
		return TOTAL_DOF * sizeof(float);
	}

	void* GetRawActionBuffer() const override {
		return (void*)CommandArray;
	}

private:

	/**
	 * ApplyTorques
	 * Applies torques for that tick on each joint with a force/direction
	 * corresponding to the values in the command array
	 */
	void ApplyTorques();

	/**
	 * ApplyLevitation
	 * Makes the HandAgent fly around. Used with the HandAgentMaxTorqueFloat control scheme
	 */
	void ApplyLevitation();

	float CommandArray[TOTAL_DOF];
};
