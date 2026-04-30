// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with,
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0),
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "MjTwistController.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;

/**
 * Captures WASD/gamepad input and stores twist commands (vx, vy, yaw_rate)
 * for broadcasting over ZMQ. Add to an AMjArticulation to enable possession-based control.
 *
 * Thread-safe: game thread writes twist state via input callbacks,
 * physics thread reads via GetTwist() for ZMQ broadcast.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjTwistController : public UActorComponent
{
	GENERATED_BODY()

public:
	UMjTwistController();

	// ─── Input Assets ───

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
	UInputMappingContext* TwistMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
	UInputAction* MoveAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
	UInputAction* TurnAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
	TArray<UInputAction*> ActionKeys;

	// ─── Velocity Limits ───

	/** Maximum forward/backward speed (m/s). Full stick or W/S produces this velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Twist")
	float MaxVx = 0.8f;

	/** Maximum strafe speed (m/s). Full stick or A/D produces this velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Twist")
	float MaxVy = 0.5f;

	/** Maximum yaw rate (rad/s). Full stick or Q/E produces this rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Twist")
	float MaxYawRate = 1.57f;

	// ─── Thread-Safe Accessors ───

	/** Returns current twist as FVector(Vx, Vy, YawRate) in m/s and rad/s. Thread-safe. */
	FVector GetTwist() const;

	/** Returns bitmask of currently pressed action keys (bits 0-9). Thread-safe. */
	int32 GetActiveActions() const;

	/** Bind input actions to an EnhancedInputComponent. Called from AMjArticulation::SetupPlayerInputComponent. */
	void BindInput(UEnhancedInputComponent* EIC);

	/** Zero all twist state. Called when unpossessed. */
	void ResetTwist();

private:
	// Input handlers
	void OnMove(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnTurn(const FInputActionValue& Value);
	void OnTurnCompleted(const FInputActionValue& Value);
	void OnActionPressed(const FInputActionValue& Value, int32 Index);
	void OnActionReleased(const FInputActionValue& Value, int32 Index);

	mutable FCriticalSection TwistMutex;
	float Vx = 0.f;
	float Vy = 0.f;
	float YawRate = 0.f;
	int32 ActionBitmask = 0;
};
