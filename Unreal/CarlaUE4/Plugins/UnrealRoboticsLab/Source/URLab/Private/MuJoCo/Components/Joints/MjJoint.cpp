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

#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "XmlNode.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"

UMjJoint::UMjJoint()
{
	PrimaryComponentTick.bCanEverTick = true;
    // MuJoCo builtin default axis is (0, 0, 1) — matches header initializer
    Type = EMjJointType::Hinge;
}


void UMjJoint::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;

    // Type
    FString TypeStr = Node->GetAttribute(TEXT("type"));
    bOverride_Type = !TypeStr.IsEmpty();
    if (bOverride_Type)
    {
        if (TypeStr == "free") Type = EMjJointType::Free;
        else if (TypeStr == "ball") Type = EMjJointType::Ball;
        else if (TypeStr == "slide") Type = EMjJointType::Slide;
        else if (TypeStr == "hinge") Type = EMjJointType::Hinge;
    }
    
    // Class Name
    MjXmlUtils::ReadAttrString(Node, TEXT("class"), MjClassName);

    // Axis — convert from MuJoCo to UE direction-vector convention (negate Y, no scale)
    FString AxisStr;
    if (MjXmlUtils::ReadAttrString(Node, TEXT("axis"), AxisStr))
    {
        bOverride_Axis = true;
        FVector RawAxis = MjXmlUtils::ParseVector(AxisStr);
        Axis = FVector(RawAxis.X, -RawAxis.Y, RawAxis.Z);
    }

    MjXmlUtils::ReadAttrFloat(Node, TEXT("ref"), Ref, bOverride_Ref);

    // Slide joints: convert ref from meters to cm (UE units)
    if (bOverride_Ref && Type == EMjJointType::Slide)
    {
        Ref *= 100.0f;
    }

    FString PosStr;
    if (MjXmlUtils::ReadAttrString(Node, TEXT("pos"), PosStr))
    {
        FVector RawPos = MjXmlUtils::ParseVector(PosStr);
        double p[3] = { (double)RawPos.X, (double)RawPos.Y, (double)RawPos.Z };
        SetRelativeLocation(MjUtils::MjToUEPosition(p));
    }

    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("stiffness"), Stiffness, bOverride_Stiffness);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("damping"), Damping, bOverride_Damping);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("armature"), Armature, bOverride_Armature);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("frictionloss"), FrictionLoss, bOverride_FrictionLoss);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("springref"), SpringRef, bOverride_SpringRef);

    // springdamper="stiffness damping" — convenience attribute that sets both at once
    {
        TArray<float> SD;
        bool bHasSD = false;
        MjXmlUtils::ReadAttrFloatArray(Node, TEXT("springdamper"), SD, bHasSD);
        if (bHasSD && SD.Num() >= 2)
        {
            Stiffness = {SD[0]};
            bOverride_Stiffness = true;
            Damping = {SD[1]};
            bOverride_Damping = true;
        }
    }
    MjXmlUtils::ReadAttrBool(Node, TEXT("limited"), bLimited, bOverride_Limited);

    // Note: autolimits (limited inferred from range) is handled by the MuJoCo spec
    // compiler (mjLIMITED_AUTO default). We do not replicate that logic here.
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("range"), Range, bOverride_Range);

    if (bOverride_Range)
    {
        if (Type == EMjJointType::Slide)
        {
            // Slide joints: MJCF stores metres, UE units are cm.
            for (float& V : Range) V *= 100.0f;
        }
        else if (CompilerSettings.bAngleInDegrees)
        {
            // Hinge/ball joint ranges in MJCF are in the compiler angle units.
            // Our spec is compiled with radians, so convert here to match.
            for (float& V : Range) V = FMath::DegreesToRadians(V);
        }
    }

    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("actuatorfrcrange"), ActuatorFrcRange, bOverride_ActuatorFrcRange);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("margin"), Margin, bOverride_Margin);

    // Solvers
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solreflimit"), SolRefLimit, bOverride_SolRefLimit);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimplimit"), SolImpLimit, bOverride_SolImpLimit);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solreffriction"), SolRefFriction, bOverride_SolRefFriction);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimpfriction"), SolImpFriction, bOverride_SolImpFriction);

    // Fallback: generic solref/solimp → limit-specific if not already set
    if (!bOverride_SolRefLimit)
        MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solref"), SolRefLimit, bOverride_SolRefLimit);
    if (!bOverride_SolImpLimit)
        MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimp"), SolImpLimit, bOverride_SolImpLimit);

    MjXmlUtils::ReadAttrInt(Node, TEXT("group"), Group, bOverride_Group);

    UE_LOG(LogURLabImport, Log, TEXT("[MjJoint XML Import] '%s' (MjName: '%s') -> Type: %s (Overridden: %s), Stiffness: %s (%f)"),
        *GetName(), *MjName,
        *StaticEnum<EMjJointType>()->GetNameStringByValue((int64)Type), bOverride_Type ? TEXT("True") : TEXT("False"),
        bOverride_Stiffness ? TEXT("Set") : TEXT("Inherited"), Stiffness.Num() > 0 ? Stiffness[0] : 0.0f);
}

void UMjJoint::ExportTo(mjsJoint* Joint, mjsDefault* Default)
{
    if (!Joint) return;

    if (bOverride_Type)
    {
        switch (Type)
        {
        case EMjJointType::Free:  Joint->type = mjJNT_FREE;  break;
        case EMjJointType::Ball:  Joint->type = mjJNT_BALL;  break;
        case EMjJointType::Slide: Joint->type = mjJNT_SLIDE; break;
        case EMjJointType::Hinge: Joint->type = mjJNT_HINGE; break;
        }
    }

    FVector _pos = GetRelativeLocation();
    if (!_pos.IsZero()) MjUtils::UEToMjPosition(_pos, Joint->pos);

    if (bOverride_Axis)
    {
        // Convert UE direction vector back to MuJoCo convention (negate Y, no scale)
        Joint->axis[0] = Axis.X;
        Joint->axis[1] = -Axis.Y;
        Joint->axis[2] = Axis.Z;
    }

    // Slide joints: convert ref from cm (UE) back to meters (MuJoCo)
    if (bOverride_Ref)
    {
        Joint->ref = (Type == EMjJointType::Slide) ? Ref / 100.0f : Ref;
    }

    if (bOverride_Stiffness)
    {
        for (int i = 0; i < Stiffness.Num() && i < (1 + mjNPOLY); ++i)
            Joint->stiffness[i] = Stiffness[i];
    }
    if (bOverride_Damping)
    {
        for (int i = 0; i < Damping.Num() && i < (1 + mjNPOLY); ++i)
            Joint->damping[i] = Damping[i];
    }
    if (bOverride_Armature) Joint->armature = Armature;
    if (bOverride_FrictionLoss) Joint->frictionloss = FrictionLoss;
    if (bOverride_SpringRef) Joint->springref = SpringRef;

    if (bOverride_Limited) Joint->limited = bLimited ? mjLIMITED_TRUE : mjLIMITED_FALSE;
    if (bOverride_Range)
    {
        // Slide joints: convert range from cm (UE) back to meters (MuJoCo)
        float Scale = (Type == EMjJointType::Slide) ? 1.0f / 100.0f : 1.0f;
        for (int i = 0; i < Range.Num() && i < 2; ++i)
        {
            Joint->range[i] = Range[i] * Scale;
        }
    }
    if (bOverride_ActuatorFrcRange)
    {
        for (int i = 0; i < ActuatorFrcRange.Num() && i < 2; ++i)
        {
            Joint->actfrcrange[i] = ActuatorFrcRange[i];
        }
    }
    
    if (bOverride_Margin) Joint->margin = Margin;

    if (bOverride_SolRefLimit)
    {
        for (int i = 0; i < SolRefLimit.Num() && i < 2; ++i)
        {
            Joint->solref_limit[i] = SolRefLimit[i];
        }
    }
    
    if (bOverride_SolImpLimit)
    {
        for (int i = 0; i < SolImpLimit.Num() && i < 5; ++i)
        {
            Joint->solimp_limit[i] = SolImpLimit[i];
        }
    }

    if (bOverride_SolRefFriction)
    {
        for (int i = 0; i < SolRefFriction.Num() && i < 2; ++i)
        {
            Joint->solref_friction[i] = SolRefFriction[i];
        }
    }
    
    if (bOverride_SolImpFriction)
    {
        for (int i = 0; i < SolImpFriction.Num() && i < 5; ++i)
        {
            Joint->solimp_friction[i] = SolImpFriction[i];
        }
    }

    if (bOverride_Group) Joint->group = Group;
}

void UMjJoint::Bind(mjModel* Model, mjData* Data, const FString& Prefix)
{
    Super::Bind(Model, Data, Prefix);
    m_JointView = BindToView<JointView>(Prefix);
    if (m_JointView.id != -1) m_ID = m_JointView.id;
    else UE_LOG(LogURLabBind, Warning, TEXT("Failed to bind Joint %s"), *GetName());
}

float UMjJoint::GetPosition() const
{
    if (m_JointView._d && m_JointView.qpos)
    {
        return (float)m_JointView.qpos[0];
    }
    return 0.0f;
}

void UMjJoint::SetPosition(float NewPosition)
{
    if (m_JointView._d && m_JointView.qpos)
    {
        m_JointView.qpos[0] = (mjtNum)NewPosition;
    }
}

float UMjJoint::GetVelocity() const
{
    if (m_JointView._d && m_JointView.qvel)
    {
        return (float)m_JointView.qvel[0];
    }
    return 0.0f;
}

void UMjJoint::SetVelocity(float NewVelocity)
{
    if (m_JointView._d && m_JointView.qvel)
    {
        m_JointView.qvel[0] = (mjtNum)NewVelocity;
    }
}

float UMjJoint::GetAcceleration() const
{
    if (m_JointView._d && m_JointView.qacc)
    {
        return (float)m_JointView.qacc[0];
    }
    return 0.0f;
}

FVector2D UMjJoint::GetJointRange() const
{
    if (m_JointView.id < 0 || !m_JointView.range) return FVector2D::ZeroVector;
    return FVector2D((float)m_JointView.range[0], (float)m_JointView.range[1]);
}

FVector UMjJoint::GetWorldAnchor() const
{
    if (m_JointView._d && m_JointView.xanchor)
    {
        return MjUtils::MjToUEPosition(m_JointView.xanchor);
    }
    return FVector::ZeroVector;
}

FVector UMjJoint::GetWorldAxis() const
{
    if (m_JointView._d && m_JointView.xaxis)
    {
        return FVector(
            (float)m_JointView.xaxis[0],
            -(float)m_JointView.xaxis[1],
            (float)m_JointView.xaxis[2]);
    }
    return FVector::ForwardVector;
}

FString UMjJoint::GetMjName() const
{
    if (m_JointView.id < 0 || !m_JointView.name) return FString();
    return MjUtils::MjToString(m_JointView.name);
}

FMuJoCoJointState UMjJoint::GetJointState() const
{
    FMuJoCoJointState State;
    if (m_JointView._d)
    {
        if (m_JointView.qpos) State.Position = (float)m_JointView.qpos[0];
        if (m_JointView.qvel) State.Velocity = (float)m_JointView.qvel[0];
        if (m_JointView.qacc) State.Acceleration = (float)m_JointView.qacc[0];
    }
    return State;
}

// --- Editor-time resolved accessors ---

EMjJointType UMjJoint::GetResolvedType() const
{
    // Runtime: use compiled model
    if (m_JointView.id >= 0)
    {
        switch (m_JointView.type)
        {
            case mjJNT_FREE:  return EMjJointType::Free;
            case mjJNT_BALL:  return EMjJointType::Ball;
            case mjJNT_SLIDE: return EMjJointType::Slide;
            case mjJNT_HINGE: return EMjJointType::Hinge;
            default:          return EMjJointType::Hinge;
        }
    }

    // Editor: local override wins
    if (bOverride_Type) return Type;

    // Walk default chain
    if (UMjDefault* Def = FindEditorDefault())
    {
        TSet<FString> Visited;
        UMjDefault* Cur = Def;
        while (Cur && !Visited.Contains(Cur->ClassName))
        {
            Visited.Add(Cur->ClassName);
            if (UMjJoint* J = Cur->FindChildOfType<UMjJoint>())
            {
                if (J->bOverride_Type) return J->Type;
            }
            Cur = Cur->ParentClassName.IsEmpty() ? nullptr
                : FindDefaultByClassName(GetOwner(), Cur->ParentClassName);
        }
    }

    // Fallback: use local Type value even without bOverride_Type.
    // This handles user-created joints where the type was set in the
    // details panel but the override toggle wasn't explicitly checked.
    // The default enum value is Hinge (MuJoCo builtin), so this is
    // safe — it returns the user's setting or the MuJoCo default.
    return Type;
}

FVector UMjJoint::GetResolvedAxis() const
{
    // Runtime: use compiled world-space axis
    if (m_JointView.id >= 0 && m_JointView.xaxis)
    {
        return FVector(
            (float)m_JointView.xaxis[0],
            -(float)m_JointView.xaxis[1],
            (float)m_JointView.xaxis[2]);
    }

    // Editor: local override wins
    if (bOverride_Axis) return Axis;

    // Walk default chain
    if (UMjDefault* Def = FindEditorDefault())
    {
        TSet<FString> Visited;
        UMjDefault* Cur = Def;
        while (Cur && !Visited.Contains(Cur->ClassName))
        {
            Visited.Add(Cur->ClassName);
            if (UMjJoint* J = Cur->FindChildOfType<UMjJoint>())
            {
                if (J->bOverride_Axis) return J->Axis;
            }
            Cur = Cur->ParentClassName.IsEmpty() ? nullptr
                : FindDefaultByClassName(GetOwner(), Cur->ParentClassName);
        }
    }

    return FVector(0, 0, 1); // MuJoCo builtin (Z-up)
}

FVector2D UMjJoint::GetResolvedRange() const
{
    // Runtime: use compiled model
    if (m_JointView.id >= 0 && m_JointView.range)
    {
        return FVector2D((float)m_JointView.range[0], (float)m_JointView.range[1]);
    }

    // Editor: local override wins
    if (bOverride_Range && Range.Num() >= 2)
    {
        return FVector2D(Range[0], Range[1]);
    }

    // Walk default chain
    if (UMjDefault* Def = FindEditorDefault())
    {
        TSet<FString> Visited;
        UMjDefault* Cur = Def;
        while (Cur && !Visited.Contains(Cur->ClassName))
        {
            Visited.Add(Cur->ClassName);
            if (UMjJoint* J = Cur->FindChildOfType<UMjJoint>())
            {
                if (J->bOverride_Range && J->Range.Num() >= 2)
                {
                    return FVector2D(J->Range[0], J->Range[1]);
                }
            }
            Cur = Cur->ParentClassName.IsEmpty() ? nullptr
                : FindDefaultByClassName(GetOwner(), Cur->ParentClassName);
        }
    }

    return FVector2D(0, 0); // MuJoCo builtin
}

bool UMjJoint::GetResolvedLimited() const
{
    // Runtime: use compiled model range
    if (m_JointView.id >= 0 && m_JointView.range)
    {
        // MuJoCo sets range to (0,0) when unlimited
        return m_JointView.range[0] != 0.0 || m_JointView.range[1] != 0.0;
    }

    // Editor: local override wins
    if (bOverride_Limited) return bLimited;

    // Walk default chain
    if (UMjDefault* Def = FindEditorDefault())
    {
        TSet<FString> Visited;
        UMjDefault* Cur = Def;
        while (Cur && !Visited.Contains(Cur->ClassName))
        {
            Visited.Add(Cur->ClassName);
            if (UMjJoint* J = Cur->FindChildOfType<UMjJoint>())
            {
                if (J->bOverride_Limited) return J->bLimited;
            }
            Cur = Cur->ParentClassName.IsEmpty() ? nullptr
                : FindDefaultByClassName(GetOwner(), Cur->ParentClassName);
        }
    }

    return false; // MuJoCo builtin
}

void UMjJoint::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    if (!ParentBody) return;

    mjsDefault* effectiveDefault = ResolveDefault(Wrapper.Spec, MjClassName);

    mjsJoint* Jnt = mjs_addJoint(ParentBody, effectiveDefault);
    m_SpecElement = Jnt->element;

    FString NameToRegister = MjName.IsEmpty() ? GetName() : MjName;
    mjs_setName(Jnt->element, TCHAR_TO_UTF8(*NameToRegister));

    ExportTo(Jnt, effectiveDefault);
}

void UMjJoint::BuildBinaryPayload(FBufferArchive& OutBuffer) const
{
    int32 JointID = m_ID;
    OutBuffer << JointID;

    float Pos = GetPosition();
    float Vel = GetVelocity();
    float Acc = GetAcceleration();

    OutBuffer << Pos;
    OutBuffer << Vel;
    OutBuffer << Acc;
}

FString UMjJoint::GetTelemetryTopicName() const
{
    return FString::Printf(TEXT("joint/%s"), *GetName());
}

#if WITH_EDITOR
TArray<FString> UMjJoint::GetDefaultClassOptions() const
{
    return GetSiblingComponentOptions(this, UMjDefault::StaticClass(), true);
}
#endif
