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

#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "Utils/URLabLogging.h"

UMjActuator::UMjActuator()
{
	PrimaryComponentTick.bCanEverTick = false;

    Type = EMjActuatorType::Motor;
    TransmissionType = EMjActuatorTrnType::Joint;
    InternalValue.store(0.0f);
    NetworkValue.store(0.0f);
}

void UMjActuator::BeginPlay()
{
	Super::BeginPlay();
}

void UMjActuator::ExportTo(mjsActuator* Actuator, mjsDefault* Default)
{
    if (!Actuator) return;

    if (!TargetName.IsEmpty()) mjs_setString(Actuator->target, TCHAR_TO_UTF8(*TargetName));

    switch(TransmissionType)
    {
        case EMjActuatorTrnType::Joint:          Actuator->trntype = mjTRN_JOINT; break;
        case EMjActuatorTrnType::JointInParent: Actuator->trntype = mjTRN_JOINTINPARENT; break;
        case EMjActuatorTrnType::SliderCrank:   Actuator->trntype = mjTRN_SLIDERCRANK; break;
        case EMjActuatorTrnType::Tendon:        Actuator->trntype = mjTRN_TENDON; break;
        case EMjActuatorTrnType::Site:          Actuator->trntype = mjTRN_SITE; break;
        case EMjActuatorTrnType::Body:          Actuator->trntype = mjTRN_BODY; break;
        default:                                Actuator->trntype = mjTRN_UNDEFINED; break;
    }

    if (TransmissionType == EMjActuatorTrnType::SliderCrank)
    {
         if (!SliderSite.IsEmpty()) mjs_setString(Actuator->slidersite, TCHAR_TO_UTF8(*SliderSite));
         Actuator->cranklength = CrankLength;
    }
    if (TransmissionType == EMjActuatorTrnType::Site && !RefSite.IsEmpty()) mjs_setString(Actuator->refsite, TCHAR_TO_UTF8(*RefSite));
    
    if (bOverride_Group) Actuator->group = Group;
    if (bOverride_ActEarly) Actuator->actearly = bActEarly ? 1 : 0;
    if (bOverride_ActLimited) Actuator->actlimited = bActLimited ? 1 : 0;

    if (bOverride_ActRange)
    {
        for (int i = 0; i < ActRange.Num() && i < 2; ++i)
        {
            Actuator->actrange[i] = ActRange[i];
        }
    }

    if (bOverride_LengthRange)
    {
        for (int i = 0; i < LengthRange.Num() && i < 2; ++i)
        {
            Actuator->lengthrange[i] = LengthRange[i];
        }
    }

    if (bOverride_CtrlLimited) Actuator->ctrllimited = bCtrlLimited ? 1 : 0;

    if (bOverride_CtrlRange)
    {
        for (int i = 0; i < CtrlRange.Num() && i < 2; ++i)
        {
            Actuator->ctrlrange[i] = CtrlRange[i];
        }
    }

    if (bOverride_ForceLimited) Actuator->forcelimited = bForceLimited ? 1 : 0;

    if (bOverride_ForceRange)
    {
        for (int i = 0; i < ForceRange.Num() && i < 2; ++i)
        {
            Actuator->forcerange[i] = ForceRange[i];
        }
    }

    if (bOverride_Gear)
    {
        for (int i=0; i < Gear.Num() && i < 6; ++i) Actuator->gear[i] = Gear[i];
    }
}

void UMjActuator::ApplyRawOverrides(mjsActuator* Actuator, mjsDefault* Default)
{
    if (!Actuator) return;

    if (bOverride_GainPrm)
    {
        for(int i=0; i<GainPrm.Num() && i<mjNGAIN; ++i) Actuator->gainprm[i] = GainPrm[i];
    }
    if (bOverride_BiasPrm)
    {
        for(int i=0; i<BiasPrm.Num() && i<mjNBIAS; ++i) Actuator->biasprm[i] = BiasPrm[i];
    }
    if (bOverride_DynPrm)
    {
        for(int i=0; i<DynPrm.Num() && i<mjNDYN; ++i) Actuator->dynprm[i] = DynPrm[i];
    }
}

void UMjActuator::ImportFromXml(const FXmlNode* Node)
{
    if (!Node) return;

    MjXmlUtils::ReadAttrString(Node, TEXT("class"), MjClassName);
    MjXmlUtils::ReadAttrInt(Node, TEXT("group"), Group, bOverride_Group);
    MjXmlUtils::ReadAttrBool(Node, TEXT("actearly"), bActEarly, bOverride_ActEarly);
    MjXmlUtils::ReadAttrBool(Node, TEXT("actlimited"), bActLimited, bOverride_ActLimited);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("actrange"), ActRange, bOverride_ActRange);
    MjXmlUtils::ReadAttrBool(Node, TEXT("ctrllimited"), bCtrlLimited, bOverride_CtrlLimited);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("ctrlrange"), CtrlRange, bOverride_CtrlRange);
    MjXmlUtils::ReadAttrBool(Node, TEXT("forcelimited"), bForceLimited, bOverride_ForceLimited);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("forcerange"), ForceRange, bOverride_ForceRange);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("lengthrange"), LengthRange, bOverride_LengthRange);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("gear"), Gear, bOverride_Gear);

    bool bDummyCrank = false;
    MjXmlUtils::ReadAttrFloat(Node, TEXT("cranklength"), CrankLength, bDummyCrank);

    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("gainprm"), GainPrm, bOverride_GainPrm);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("biasprm"), BiasPrm, bOverride_BiasPrm);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("dynprm"), DynPrm, bOverride_DynPrm);

    // Transmission detection
    FString TrnTarget;
    if (MjXmlUtils::ReadAttrString(Node, TEXT("joint"), TrnTarget))
    {
        TransmissionType = EMjActuatorTrnType::Joint;
        TargetName = TrnTarget;
    }
    if (MjXmlUtils::ReadAttrString(Node, TEXT("jointinparent"), TrnTarget))
    {
        TransmissionType = EMjActuatorTrnType::JointInParent;
        TargetName = TrnTarget;
    }
    if (MjXmlUtils::ReadAttrString(Node, TEXT("tendon"), TrnTarget))
    {
        TransmissionType = EMjActuatorTrnType::Tendon;
        TargetName = TrnTarget;
    }
    if (MjXmlUtils::ReadAttrString(Node, TEXT("slidersite"), SliderSite))
    {
        TransmissionType = EMjActuatorTrnType::SliderCrank;
    }
    if (MjXmlUtils::ReadAttrString(Node, TEXT("site"), TrnTarget))
    {
        TransmissionType = EMjActuatorTrnType::Site;
        TargetName = TrnTarget;
    }
    MjXmlUtils::ReadAttrString(Node, TEXT("refsite"), RefSite);
    if (MjXmlUtils::ReadAttrString(Node, TEXT("body"), TrnTarget))
    {
        TransmissionType = EMjActuatorTrnType::Body;
        TargetName = TrnTarget;
    }

    // Call subclass-specific parsing
    ParseSpecifics(Node);
}

void UMjActuator::Bind(mjModel* Model, mjData* Data, const FString& Prefix)
{
    Super::Bind(Model, Data, Prefix);
    m_ActuatorView = BindToView<ActuatorView>(Prefix);

    if (m_ActuatorView.id != -1)
    {
        m_ID = m_ActuatorView.id;
    }
    else
    {
        UE_LOG(LogURLabBind, Warning, TEXT("[MjActuator] Actuator '%s' could not bind! Prefix: %s"), *GetName(), *Prefix);
    }
}

// ----------------------------------------------------------------------------------
// Blueprint Runtime API — Setup & Control
// ----------------------------------------------------------------------------------

void UMjActuator::SetControl(float Value)
{
    InternalValue.store(Value);
}

void UMjActuator::SetNetworkControl(float Value)
{
    NetworkValue.store(Value);
}

void UMjActuator::ResetControl()
{
    InternalValue.store(0.0f);
    NetworkValue.store(0.0f);
}

float UMjActuator::GetControl() const
{
    if (AMjArticulation* Art = Cast<AMjArticulation>(GetOwner()))
    {
        return ResolveDesiredControl(Art->ControlSource);
    }

    if (AAMjManager* Mgr = AAMjManager::GetManager())
    {
        if (Mgr->PhysicsEngine)
            return ResolveDesiredControl((uint8)Mgr->PhysicsEngine->ControlSource);
    }
    return InternalValue.load();
}

float UMjActuator::GetMjControl() const
{
    return (m_ActuatorView.ctrl) ? (float)m_ActuatorView.ctrl[0] : 0.0f;
}

float UMjActuator::ResolveDesiredControl(uint8 Source) const
{
    // Source: 0 = ZMQ, 1 = UI (matching EControlSource)
    if (Source == 0) 
    {
        return NetworkValue.load();
    }
    return InternalValue.load();
}

float UMjActuator::GetForce() const
{
    if (m_ActuatorView.id != -1 && m_ActuatorView.force)
    {
        return (float)m_ActuatorView.force[0];
    }
    return 0.0f;
}

float UMjActuator::GetLength() const
{
    if (m_ActuatorView.id != -1 && m_ActuatorView.length)
    {
        return (float)m_ActuatorView.length[0];
    }
    return 0.0f;
}

float UMjActuator::GetVelocity() const
{
    if (m_ActuatorView.id != -1 && m_ActuatorView.velocity)
    {
        return (float)m_ActuatorView.velocity[0];
    }
    return 0.0f;
}

void UMjActuator::SetGear(const TArray<float>& NewGear)
{
    if (m_ActuatorView.id != -1 && m_ActuatorView.gear)
    {
        for(int i=0; i<NewGear.Num() && i<6; ++i)
        {
             m_ActuatorView.gear[i] = (mjtNum)NewGear[i];
        }
    }
}

TArray<float> UMjActuator::GetGear() const
{
    TArray<float> Res;
    if (m_ActuatorView.id != -1 && m_ActuatorView.gear)
    {
        for(int i=0; i<6; ++i) Res.Add((float)m_ActuatorView.gear[i]);
    }
    return Res;
}

FVector2D UMjActuator::GetControlRange() const
{
    if (m_ActuatorView.id < 0 || !m_ActuatorView.ctrlrange) return FVector2D::ZeroVector;
    return FVector2D((float)m_ActuatorView.ctrlrange[0], (float)m_ActuatorView.ctrlrange[1]);
}

float UMjActuator::GetActivation() const
{
    if (m_ActuatorView.id < 0 || !m_ActuatorView.act) return 0.0f;
    return (float)m_ActuatorView.act[0];
}

FString UMjActuator::GetMjName() const
{
    if (m_ActuatorView.id < 0 || !m_ActuatorView.name) return FString();
    return MjUtils::MjToString(m_ActuatorView.name);
}

// ----------------------------------------------------------------------------------
// Base Export (Common Properties & Virtual Dispatch)
// ----------------------------------------------------------------------------------

void UMjActuator::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    if (m_SpecElement) return;

    mjsDefault* effectiveDefault = ResolveDefault(Wrapper.Spec, MjClassName);

    const char* defName = effectiveDefault ? mjs_getString(mjs_getName(effectiveDefault->element)) : "NULL";
    UE_LOG(LogURLabBind, Verbose, TEXT("[MjActuator::RegisterToSpec] '%s' class='%s' -> resolved default='%s'"),
        *GetName(), *MjClassName, UTF8_TO_TCHAR(defName ? defName : "unnamed"));

    mjsActuator* act = mjs_addActuator(Wrapper.Spec, effectiveDefault);
    m_SpecElement = act->element;
    SetSpecElementName(Wrapper, act->element, mjOBJ_ACTUATOR);

    UE_LOG(LogURLabBind, Verbose, TEXT("[MjActuator::RegisterToSpec] '%s' after mjs_addActuator: ctrlrange=[%.3f, %.3f], ctrllimited=%d"),
        *GetName(), act->ctrlrange[0], act->ctrlrange[1], act->ctrllimited);

    ExportTo(act, effectiveDefault);
}

#if WITH_EDITOR
TArray<FString> UMjActuator::GetTargetNameOptions() const
{
    UClass* FilterClass = nullptr;
    switch (TransmissionType)
    {
        case EMjActuatorTrnType::Joint:
        case EMjActuatorTrnType::JointInParent:
            FilterClass = UMjJoint::StaticClass();
            break;
        case EMjActuatorTrnType::Tendon:
            FilterClass = UMjTendon::StaticClass();
            break;
        case EMjActuatorTrnType::Site:
        case EMjActuatorTrnType::SliderCrank:
            FilterClass = UMjSite::StaticClass();
            break;
        case EMjActuatorTrnType::Body:
            FilterClass = UMjBody::StaticClass();
            break;
        default:
            FilterClass = UMjJoint::StaticClass();
            break;
    }
    return GetSiblingComponentOptions(this, FilterClass);
}

TArray<FString> UMjActuator::GetSliderSiteOptions() const
{
    return GetSiblingComponentOptions(this, UMjSite::StaticClass());
}

TArray<FString> UMjActuator::GetRefSiteOptions() const
{
    return GetSiblingComponentOptions(this, UMjSite::StaticClass());
}

TArray<FString> UMjActuator::GetDefaultClassOptions() const
{
    return GetSiblingComponentOptions(this, UMjDefault::StaticClass(), true);
}
#endif

