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

#include "MuJoCo/Components/Constraints/MjEquality.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "Utils/URLabLogging.h"

UMjEquality::UMjEquality()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMjEquality::ImportFromXml(const FXmlNode* Node)
{
    if (!Node) return;

    FString Tag = Node->GetTag().ToLower();
    if (Tag == TEXT("connect"))          EqualityType = EMjEqualityType::Connect;
    else if (Tag == TEXT("weld"))        EqualityType = EMjEqualityType::Weld;
    else if (Tag == TEXT("joint"))       EqualityType = EMjEqualityType::Joint;
    else if (Tag == TEXT("tendon"))      EqualityType = EMjEqualityType::Tendon;
    else if (Tag == TEXT("flex"))        EqualityType = EMjEqualityType::Flex;
    else if (Tag == TEXT("flexvert"))    EqualityType = EMjEqualityType::FlexVert;
    else if (Tag == TEXT("flexstrain"))  EqualityType = EMjEqualityType::FlexStrain;

    Obj1 = Node->GetAttribute(TEXT("body1"));
    if (Obj1.IsEmpty()) Obj1 = Node->GetAttribute(TEXT("joint1"));
    if (Obj1.IsEmpty()) Obj1 = Node->GetAttribute(TEXT("tendon1"));
    if (Obj1.IsEmpty()) Obj1 = Node->GetAttribute(TEXT("flex"));  // flex / flexvert / flexstrain target a single flex

    Obj2 = Node->GetAttribute(TEXT("body2"));
    if (Obj2.IsEmpty()) Obj2 = Node->GetAttribute(TEXT("joint2"));
    if (Obj2.IsEmpty()) Obj2 = Node->GetAttribute(TEXT("tendon2"));

    {
        bool bActiveOverride = false;
        MjXmlUtils::ReadAttrBool(Node, TEXT("active"), bActive, bActiveOverride);
    }

    // --- Physics ---
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solref"), SolRef, bOverride_SolRef);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimp"), SolImp, bOverride_SolImp);

    // --- Type Specific ---
    FString AnchorStr = Node->GetAttribute(TEXT("anchor"));
    bOverride_Anchor = !AnchorStr.IsEmpty();
    if (bOverride_Anchor)
    {
        TArray<FString> Parts;
        AnchorStr.ParseIntoArray(Parts, TEXT(" "), true);
        if (Parts.Num() >= 3)
            Anchor = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2])) * 100.0f;
    }

    if (EqualityType == EMjEqualityType::Weld)
    {
        FString RelPosStr = Node->GetAttribute(TEXT("relpos"));
        FString RelQuatStr = Node->GetAttribute(TEXT("relquat"));
        if (!RelPosStr.IsEmpty() || !RelQuatStr.IsEmpty())
        {
            bOverride_RelPose = true;
            FVector P = FVector::ZeroVector;
            FQuat Q = FQuat::Identity;
            
            if (!RelPosStr.IsEmpty())
            {
                TArray<FString> Parts;
                RelPosStr.ParseIntoArray(Parts, TEXT(" "), true);
                if (Parts.Num() >= 3)
                    P = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2])) * 100.0f;
            }
            if (!RelQuatStr.IsEmpty())
            {
                TArray<FString> Parts;
                RelQuatStr.ParseIntoArray(Parts, TEXT(" "), true);
                if (Parts.Num() >= 4)
                {
                   double mq[4] = { FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]), FCString::Atod(*Parts[3]) };
                   Q = MjUtils::MjToUERotation(mq);
                }
            }
            RelPose = FTransform(Q, P);
        }
    }

    if (EqualityType == EMjEqualityType::Weld)
    {
        FString TorqueScaleStr = Node->GetAttribute(TEXT("torquescale"));
        if (!TorqueScaleStr.IsEmpty())
        {
            bOverride_TorqueScale = true;
            TorqueScale = FCString::Atof(*TorqueScaleStr);
        }
    }

    FString PolyCoefStr = Node->GetAttribute(TEXT("polycoef"));
    bOverride_PolyCoef = !PolyCoefStr.IsEmpty();
    if (bOverride_PolyCoef)
    {
        TArray<FString> Parts;
        PolyCoefStr.ParseIntoArray(Parts, TEXT(" "), true);
        PolyCoef.Empty();
        for (const FString& P : Parts) PolyCoef.Add(FCString::Atof(*P));
    }

    UE_LOG(LogURLabImport, Log, TEXT("[MjEquality XML Import] '%s' (%d) | Obj1: %s, Obj2: %s"), 
        *GetName(), (int)EqualityType, *Obj1, *Obj2);
}

void UMjEquality::ExportTo(mjsEquality* Eq)
{
    if (!Eq) return;

    Eq->type = (mjtEq)EqualityType;

    if (!Obj1.IsEmpty()) mjs_setString(Eq->name1, TCHAR_TO_UTF8(*Obj1));
    if (!Obj2.IsEmpty()) mjs_setString(Eq->name2, TCHAR_TO_UTF8(*Obj2));

    Eq->active = bActive ? 1 : 0;

    if (bOverride_SolRef)
    {
        for (int i = 0; i < SolRef.Num() && i < 2; ++i)
        {
            Eq->solref[i] = SolRef[i];
        }
    }
    if (bOverride_SolImp)
    {
        for (int i = 0; i < SolImp.Num() && i < 5; ++i)
        {
            Eq->solimp[i] = SolImp[i];
        }
    }

    // Data mapping
    switch (EqualityType)
    {
    case EMjEqualityType::Connect:
        if (bOverride_Anchor)
        {
            double mp[3];
            MjUtils::UEToMjPosition(Anchor, mp);
            Eq->data[0] = mp[0];
            Eq->data[1] = mp[1];
            Eq->data[2] = mp[2];
        }
        break;

    case EMjEqualityType::Weld:
        if (bOverride_RelPose)
        {
            double mp[3];
            double mq[4];
            MjUtils::UEToMjPosition(RelPose.GetLocation(), mp);
            MjUtils::UEToMjRotation(RelPose.GetRotation(), mq);
            Eq->data[0] = mp[0];
            Eq->data[1] = mp[1];
            Eq->data[2] = mp[2];
            Eq->data[3] = mq[0]; // w
            Eq->data[4] = mq[1]; // x
            Eq->data[5] = mq[2]; // y
            Eq->data[6] = mq[3]; // z
        }
        // data[7] = torquescale (MuJoCo 3.x weld equality, mjSpec)
        if (bOverride_TorqueScale)
        {
            Eq->data[7] = TorqueScale;
        }
        break;

    case EMjEqualityType::Joint:
    case EMjEqualityType::Tendon:
        if (bOverride_PolyCoef)
        {
            for (int i = 0; i < PolyCoef.Num() && i < 11; ++i)
            {
                Eq->data[i] = PolyCoef[i];
            }
        }
        break;
    }
}

void UMjEquality::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    mjsEquality* Eq = mjs_addEquality(Wrapper.Spec, nullptr);
    if (!Eq) return;

    m_SpecElement = Eq->element;

    FString TName = MjName.IsEmpty() ? GetName() : MjName;
    mjs_setName(Eq->element, TCHAR_TO_UTF8(*TName));

    ExportTo(Eq);
}

#if WITH_EDITOR
TArray<FString> UMjEquality::GetObjOptions() const
{
    UClass* FilterClass = nullptr;
    switch (EqualityType)
    {
        case EMjEqualityType::Connect:
        case EMjEqualityType::Weld:
            FilterClass = UMjBody::StaticClass();
            break;
        case EMjEqualityType::Joint:
            FilterClass = UMjJoint::StaticClass();
            break;
        case EMjEqualityType::Tendon:
            FilterClass = UMjTendon::StaticClass();
            break;
    }
    if (!FilterClass) return {TEXT("")};
    return UMjComponent::GetSiblingComponentOptions(this, FilterClass);
}
#endif
