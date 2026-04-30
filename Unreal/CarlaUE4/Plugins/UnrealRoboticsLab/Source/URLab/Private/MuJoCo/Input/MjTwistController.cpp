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

#include "MuJoCo/Input/MjTwistController.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"

UMjTwistController::UMjTwistController()
{
	PrimaryComponentTick.bCanEverTick = false;
	ActionKeys.SetNum(10);
}

FVector UMjTwistController::GetTwist() const
{
	FScopeLock Lock(&TwistMutex);
	return FVector(Vx, Vy, YawRate);
}

int32 UMjTwistController::GetActiveActions() const
{
	FScopeLock Lock(&TwistMutex);
	return ActionBitmask;
}

void UMjTwistController::ResetTwist()
{
	FScopeLock Lock(&TwistMutex);
	Vx = 0.f;
	Vy = 0.f;
	YawRate = 0.f;
	ActionBitmask = 0;
}

void UMjTwistController::BindInput(UEnhancedInputComponent* EIC)
{
	if (!EIC) return;

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UMjTwistController::OnMove);
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &UMjTwistController::OnMoveCompleted);
	}

	if (TurnAction)
	{
		EIC->BindAction(TurnAction, ETriggerEvent::Triggered, this, &UMjTwistController::OnTurn);
		EIC->BindAction(TurnAction, ETriggerEvent::Completed, this, &UMjTwistController::OnTurnCompleted);
	}

	for (int32 i = 0; i < ActionKeys.Num() && i < 10; ++i)
	{
		if (ActionKeys[i])
		{
			EIC->BindAction(ActionKeys[i], ETriggerEvent::Started, this,
				&UMjTwistController::OnActionPressed, i);
			EIC->BindAction(ActionKeys[i], ETriggerEvent::Completed, this,
				&UMjTwistController::OnActionReleased, i);
		}
	}
}

void UMjTwistController::OnMove(const FInputActionValue& Value)
{
	FVector2D Axis = Value.Get<FVector2D>();
	FScopeLock Lock(&TwistMutex);
	// Y axis = forward/backward, X axis = strafe (standard WASD convention)
	Vx = FMath::Clamp(Axis.Y * MaxVx, -MaxVx, MaxVx);
	Vy = FMath::Clamp(Axis.X * MaxVy, -MaxVy, MaxVy);
}

void UMjTwistController::OnMoveCompleted(const FInputActionValue& Value)
{
	FScopeLock Lock(&TwistMutex);
	Vx = 0.f;
	Vy = 0.f;
}

void UMjTwistController::OnTurn(const FInputActionValue& Value)
{
	float Axis = Value.Get<float>();
	FScopeLock Lock(&TwistMutex);
	YawRate = FMath::Clamp(Axis * MaxYawRate, -MaxYawRate, MaxYawRate);
}

void UMjTwistController::OnTurnCompleted(const FInputActionValue& Value)
{
	FScopeLock Lock(&TwistMutex);
	YawRate = 0.f;
}

void UMjTwistController::OnActionPressed(const FInputActionValue& Value, int32 Index)
{
	if (Index < 0 || Index >= 10) return;
	FScopeLock Lock(&TwistMutex);
	ActionBitmask |= (1 << Index);
}

void UMjTwistController::OnActionReleased(const FInputActionValue& Value, int32 Index)
{
	if (Index < 0 || Index >= 10) return;
	FScopeLock Lock(&TwistMutex);
	ActionBitmask &= ~(1 << Index);
}
