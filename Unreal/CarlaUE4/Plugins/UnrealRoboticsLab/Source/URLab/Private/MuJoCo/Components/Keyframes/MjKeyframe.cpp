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

#include "MuJoCo/Components/Keyframes/MjKeyframe.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Joints/MjFreeJoint.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "XmlNode.h"
#include "Utils/URLabLogging.h"

UMjKeyframe::UMjKeyframe()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMjKeyframe::ImportFromXml(const FXmlNode* Node)
{
    if (!Node) return;

    {
        bool bTimeOverride = false;
        MjXmlUtils::ReadAttrFloat(Node, TEXT("time"), Time, bTimeOverride);
    }

    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("qpos"),  Qpos,  bOverride_Qpos);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("qvel"),  Qvel,  bOverride_Qvel);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("act"),   Act,   bOverride_Act);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("ctrl"),  Ctrl,  bOverride_Ctrl);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("mpos"),  Mpos,  bOverride_Mpos);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("mquat"), Mquat, bOverride_Mquat);

    UE_LOG(LogURLabImport, Log, TEXT("[MjKeyframe XML Import] '%s' | Time: %f, Qpos: %d, Qvel: %d"), 
        *GetName(), Time, Qpos.Num(), Qvel.Num());
}

void UMjKeyframe::ExportTo(mjsKey* Key)
{
    if (!Key) return;

    Key->time = (double)Time;

    auto SetVec = [](mjDoubleVec* Dest, const TArray<float>& Src) {
        if (!Dest) return;
        Dest->clear();
        for (float V : Src) Dest->push_back((double)V);
    };

    if (bOverride_Qpos) SetVec(Key->qpos, Qpos);
    if (bOverride_Qvel) SetVec(Key->qvel, Qvel);
    if (bOverride_Act)  SetVec(Key->act, Act);
    if (bOverride_Ctrl) SetVec(Key->ctrl, Ctrl);
    if (bOverride_Mpos) SetVec(Key->mpos, Mpos);
    if (bOverride_Mquat) SetVec(Key->mquat, Mquat);
}

// Counts expected DOF dimensions from the owning actor's joint components.
struct FKeyframeDimensions
{
    int32 NumFreeJoints = 0;
    int32 NqJointsOnly = 0;   // hinge=1, slide=1, ball=4
    int32 NvJointsOnly = 0;   // hinge=1, slide=1, ball=3
    int32 NqWithFree = 0;     // NqJointsOnly + NumFreeJoints * 7
    int32 NvWithFree = 0;     // NvJointsOnly + NumFreeJoints * 6
    int32 NumActuators = 0;
};

static FKeyframeDimensions CountDimensions(AActor* Owner)
{
    FKeyframeDimensions D;
    if (!Owner) return D;

    TArray<UActorComponent*> Components;
    Owner->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (Cast<UMjFreeJoint>(Comp))
        {
            D.NumFreeJoints++;
            continue;
        }
        if (UMjJoint* Joint = Cast<UMjJoint>(Comp))
        {
            if (Joint->bIsDefault) continue;
            if (Joint->Type == EMjJointType::Free)
            {
                D.NumFreeJoints++;
            }
            else if (Joint->Type == EMjJointType::Hinge)
            {
                D.NqJointsOnly += 1;
                D.NvJointsOnly += 1;
            }
            else if (Joint->Type == EMjJointType::Slide)
            {
                D.NqJointsOnly += 1;
                D.NvJointsOnly += 1;
            }
            else if (Joint->Type == EMjJointType::Ball)
            {
                D.NqJointsOnly += 4;
                D.NvJointsOnly += 3;
            }
        }
        if (Cast<UMjActuator>(Comp))
        {
            UMjComponent* MjComp = Cast<UMjComponent>(Comp);
            if (MjComp && !MjComp->bIsDefault)
                D.NumActuators++;
        }
    }

    D.NqWithFree = D.NqJointsOnly + D.NumFreeJoints * 7;
    D.NvWithFree = D.NvJointsOnly + D.NumFreeJoints * 6;
    return D;
}

void UMjKeyframe::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    mjsKey* Key = mjs_addKey(Wrapper.Spec);
    if (!Key)
    {
        UE_LOG(LogURLabImport, Error, TEXT("[MjKeyframe] mjs_addKey failed for '%s'"), *GetName());
        return;
    }

    m_SpecElement = Key->element;

    FString TName = MjName.IsEmpty() ? GetName() : MjName;
    mjs_setName(Key->element, TCHAR_TO_UTF8(*TName));

    Key->time = (double)Time;

    auto SetVec = [](mjDoubleVec* Dest, const TArray<float>& Src) {
        if (!Dest) return;
        Dest->clear();
        for (float V : Src) Dest->push_back((double)V);
    };

    FKeyframeDimensions Dims = CountDimensions(GetOwner());

    // --- Qpos: size-based detection of freejoint presence ---
    if (bOverride_Qpos)
    {
        if (Dims.NumFreeJoints > 0 && Qpos.Num() == Dims.NqJointsOnly)
        {
            // XML has joints only — pad with freejoint defaults
            TArray<float> Padded;
            Padded.Reserve(Dims.NqWithFree);
            for (int32 i = 0; i < Dims.NumFreeJoints; i++)
            {
                Padded.Append({0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f});
            }
            Padded.Append(Qpos);
            SetVec(Key->qpos, Padded);
            UE_LOG(LogURLabImport, Log, TEXT("[MjKeyframe] '%s': padded qpos %d → %d (%d free joints)"),
                *TName, Qpos.Num(), Padded.Num(), Dims.NumFreeJoints);
        }
        else if (Dims.NumFreeJoints > 0 && Qpos.Num() == Dims.NqWithFree)
        {
            // XML already includes freejoint values — export as-is
            SetVec(Key->qpos, Qpos);
            UE_LOG(LogURLabImport, Verbose, TEXT("[MjKeyframe] '%s': qpos already has freejoint (%d values), no padding"),
                *TName, Qpos.Num());
        }
        else
        {
            // No freejoints, or unexpected size — export as-is
            if (Dims.NumFreeJoints > 0 && Qpos.Num() != Dims.NqJointsOnly && Qpos.Num() != Dims.NqWithFree)
            {
                UE_LOG(LogURLabImport, Warning, TEXT("[MjKeyframe] '%s': qpos has %d values, expected %d (with free) or %d (joints only)"),
                    *TName, Qpos.Num(), Dims.NqWithFree, Dims.NqJointsOnly);
            }
            SetVec(Key->qpos, Qpos);
        }
    }

    // --- Qvel: same size-based detection ---
    if (bOverride_Qvel)
    {
        if (Dims.NumFreeJoints > 0 && Qvel.Num() == Dims.NvJointsOnly)
        {
            // Joints only — pad with freejoint zero velocities
            TArray<float> Padded;
            Padded.Reserve(Dims.NvWithFree);
            for (int32 i = 0; i < Dims.NumFreeJoints; i++)
            {
                Padded.Append({0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
            }
            Padded.Append(Qvel);
            SetVec(Key->qvel, Padded);
            UE_LOG(LogURLabImport, Log, TEXT("[MjKeyframe] '%s': padded qvel %d → %d"),
                *TName, Qvel.Num(), Padded.Num());
        }
        else if (Dims.NumFreeJoints > 0 && Qvel.Num() == Dims.NvWithFree)
        {
            SetVec(Key->qvel, Qvel);
        }
        else
        {
            if (Dims.NumFreeJoints > 0 && Qvel.Num() != Dims.NvJointsOnly && Qvel.Num() != Dims.NvWithFree)
            {
                UE_LOG(LogURLabImport, Warning, TEXT("[MjKeyframe] '%s': qvel has %d values, expected %d (with free) or %d (joints only)"),
                    *TName, Qvel.Num(), Dims.NvWithFree, Dims.NvJointsOnly);
            }
            SetVec(Key->qvel, Qvel);
        }
    }

    // --- Ctrl: validate size ---
    if (bOverride_Ctrl)
    {
        if (Dims.NumActuators > 0 && Ctrl.Num() != Dims.NumActuators)
        {
            UE_LOG(LogURLabImport, Warning, TEXT("[MjKeyframe] '%s': ctrl has %d values but %d actuators detected"),
                *TName, Ctrl.Num(), Dims.NumActuators);
        }
        SetVec(Key->ctrl, Ctrl);
    }

    // --- Remaining arrays: no freejoint involvement ---
    if (bOverride_Act)   SetVec(Key->act, Act);
    if (bOverride_Mpos)  SetVec(Key->mpos, Mpos);
    if (bOverride_Mquat) SetVec(Key->mquat, Mquat);

    UE_LOG(LogURLabImport, Log, TEXT("[MjKeyframe] Registered '%s' at time %f (nq_joints=%d, nq_free=%d, nv_joints=%d, nv_free=%d, nu=%d)"),
        *TName, Time, Dims.NqJointsOnly, Dims.NqWithFree, Dims.NvJointsOnly, Dims.NvWithFree, Dims.NumActuators);
}
