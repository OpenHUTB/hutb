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

#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "mujoco/mujoco.h"
#include "mujoco/mjspec.h"
#include "MuJoCo/Components/Defaults/MujocoDefaults.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "XmlNode.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Utils/MjXmlUtils.h"


UMjTendon::UMjTendon()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMjTendon::BeginPlay()
{
    Super::BeginPlay();
}

void UMjTendon::ImportFromXml(const FXmlNode* Node)
{
    if (!Node) return;

    // Class name
    MjXmlUtils::ReadAttrString(Node, TEXT("class"), MjClassName);

    // --- Physics ---
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("stiffness"), Stiffness, bOverride_Stiffness);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("springlength"), SpringLength, bOverride_SpringLength);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("damping"), Damping, bOverride_Damping);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("frictionloss"), FrictionLoss, bOverride_FrictionLoss);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("armature"), Armature, bOverride_Armature);

    // --- Limits ---
    MjXmlUtils::ReadAttrBool(Node, TEXT("limited"), bLimited, bOverride_Limited);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("range"), Range, bOverride_Range);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("margin"), Margin, bOverride_Margin);
    MjXmlUtils::ReadAttrBool(Node, TEXT("actfrclimited"), bActFrcLimited, bOverride_ActFrcLimited);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("actfrcrange"), ActFrcRange, bOverride_ActFrcRange);

    // --- Solver ---
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solreflimit"), SolRefLimit, bOverride_SolRefLimit);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimplimit"), SolImpLimit, bOverride_SolImpLimit);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solreffriction"), SolRefFriction, bOverride_SolRefFriction);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimpfriction"), SolImpFriction, bOverride_SolImpFriction);

    // --- Visual ---
    MjXmlUtils::ReadAttrFloat(Node, TEXT("width"), Width, bOverride_Width);
    TArray<float> RgbaParts;
    if (MjXmlUtils::ReadAttrFloatArray(Node, TEXT("rgba"), RgbaParts, bOverride_Rgba))
    {
        if (RgbaParts.Num() >= 4)
            Rgba = FLinearColor(RgbaParts[0], RgbaParts[1], RgbaParts[2], RgbaParts[3]);
    }
    MjXmlUtils::ReadAttrInt(Node, TEXT("group"), Group, bOverride_Group);

    // --- Wrap entries (child nodes of the fixed/spatial element) ---
    Wraps.Empty();
    for (const FXmlNode* Child : Node->GetChildrenNodes())
    {
        if (!Child) continue;
        FString ChildTag = Child->GetTag();

        FMjTendonWrap Wrap;

        if (ChildTag.Equals(TEXT("joint"), ESearchCase::IgnoreCase))
        {
            Wrap.Type = EMjTendonWrapType::Joint;
            Wrap.TargetName = Child->GetAttribute(TEXT("joint"));
            FString CoefStr = Child->GetAttribute(TEXT("coef"));
            if (!CoefStr.IsEmpty()) Wrap.Coef = FCString::Atof(*CoefStr);
        }
        else if (ChildTag.Equals(TEXT("site"), ESearchCase::IgnoreCase))
        {
            Wrap.Type = EMjTendonWrapType::Site;
            Wrap.TargetName = Child->GetAttribute(TEXT("site"));
        }
        else if (ChildTag.Equals(TEXT("geom"), ESearchCase::IgnoreCase))
        {
            Wrap.Type = EMjTendonWrapType::Geom;
            Wrap.TargetName = Child->GetAttribute(TEXT("geom"));
            Wrap.SideSite = Child->GetAttribute(TEXT("sidesite"));
        }
        else if (ChildTag.Equals(TEXT("pulley"), ESearchCase::IgnoreCase))
        {
            Wrap.Type = EMjTendonWrapType::Pulley;
            FString DivisorStr = Child->GetAttribute(TEXT("divisor"));
            if (!DivisorStr.IsEmpty()) Wrap.Divisor = FCString::Atof(*DivisorStr);
        }
        else
        {
            UE_LOG(LogURLabImport, Warning, TEXT("[MjTendon XML Import] Unknown wrap tag: '%s'"), *ChildTag);
            continue;
        }

        Wraps.Add(Wrap);
    }

    UE_LOG(LogURLabImport, Log, TEXT("[MjTendon XML Import] '%s' -> %d wrap entries"), *GetName(), Wraps.Num());
}

void UMjTendon::ExportTo(mjsTendon* Tendon, mjsDefault* def)
{
    if (!Tendon) return;

    // --- Physics properties ---
    if (bOverride_Stiffness)
    {
        for (int i = 0; i < Stiffness.Num() && i < (1 + mjNPOLY); ++i)
            Tendon->stiffness[i] = Stiffness[i];
    }
    if (bOverride_SpringLength)
    {
        for (int i = 0; i < SpringLength.Num() && i < 2; ++i)
            Tendon->springlength[i] = SpringLength[i];
    }
    if (bOverride_Damping)
    {
        for (int i = 0; i < Damping.Num() && i < (1 + mjNPOLY); ++i)
            Tendon->damping[i] = Damping[i];
    }
    if (bOverride_FrictionLoss) Tendon->frictionloss = FrictionLoss;
    if (bOverride_Armature)    Tendon->armature = Armature;

    // --- Limits ---
    if (bOverride_Limited)     Tendon->limited = bLimited ? mjLIMITED_TRUE : mjLIMITED_FALSE;
    if (bOverride_Range)
    {
        for (int i = 0; i < Range.Num() && i < 2; ++i)
            Tendon->range[i] = Range[i];
    }
    if (bOverride_Margin)      Tendon->margin = Margin;

    if (bOverride_ActFrcLimited) Tendon->actfrclimited = bActFrcLimited ? mjLIMITED_TRUE : mjLIMITED_FALSE;
    if (bOverride_ActFrcRange)
    {
        for (int i = 0; i < ActFrcRange.Num() && i < 2; ++i)
            Tendon->actfrcrange[i] = ActFrcRange[i];
    }

    // --- Solver ---
    if (bOverride_SolRefLimit)
    {
        for (int i = 0; i < SolRefLimit.Num() && i < 2; ++i)
            Tendon->solref_limit[i] = SolRefLimit[i];
    }
    if (bOverride_SolImpLimit)
    {
        for (int i = 0; i < SolImpLimit.Num() && i < 5; ++i)
            Tendon->solimp_limit[i] = SolImpLimit[i];
    }
    if (bOverride_SolRefFriction)
    {
        for (int i = 0; i < SolRefFriction.Num() && i < 2; ++i)
            Tendon->solref_friction[i] = SolRefFriction[i];
    }
    if (bOverride_SolImpFriction)
    {
        for (int i = 0; i < SolImpFriction.Num() && i < 5; ++i)
            Tendon->solimp_friction[i] = SolImpFriction[i];
    }

    // --- Visuals ---
    if (bOverride_Width) Tendon->width = Width;
    if (bOverride_Rgba)
    {
        Tendon->rgba[0] = Rgba.R;
        Tendon->rgba[1] = Rgba.G;
        Tendon->rgba[2] = Rgba.B;
        Tendon->rgba[3] = Rgba.A;
    }
    if (bOverride_Group) Tendon->group = Group;
}

void UMjTendon::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    mjsDefault* effectiveDefault = ResolveDefault(Wrapper.Spec, MjClassName);

    // Create tendon in the spec
    mjsTendon* Tendon = mjs_addTendon(Wrapper.Spec, effectiveDefault);
    if (!Tendon)
    {
        UE_LOG(LogURLabImport, Error, TEXT("[MjTendon] mjs_addTendon failed for '%s'"), *GetName());
        return;
    }

    m_SpecElement = Tendon->element;

    // Set name
    FString TName = MjName.IsEmpty() ? GetName() : MjName;
    mjs_setName(Tendon->element, TCHAR_TO_UTF8(*TName));

    // Export properties
    ExportTo(Tendon, effectiveDefault);

    // --- Wrap entries ---
    for (const FMjTendonWrap& Wrap : Wraps)
    {
        switch (Wrap.Type)
        {
        case EMjTendonWrapType::Joint:
            {
                mjsWrap* W = mjs_wrapJoint(Tendon, TCHAR_TO_UTF8(*Wrap.TargetName), (double)Wrap.Coef);
                if (!W)
                {
                    UE_LOG(LogURLabImport, Warning, TEXT("[MjTendon] mjs_wrapJoint failed for joint '%s' in tendon '%s'"),
                        *Wrap.TargetName, *TName);
                }
            }
            break;

        case EMjTendonWrapType::Site:
            {
                mjsWrap* W = mjs_wrapSite(Tendon, TCHAR_TO_UTF8(*Wrap.TargetName));
                if (!W)
                {
                    UE_LOG(LogURLabImport, Warning, TEXT("[MjTendon] mjs_wrapSite failed for site '%s' in tendon '%s'"),
                        *Wrap.TargetName, *TName);
                }
            }
            break;

        case EMjTendonWrapType::Geom:
            {
                const char* SideSiteStr = Wrap.SideSite.IsEmpty() ? "" : TCHAR_TO_UTF8(*Wrap.SideSite);
                mjsWrap* W = mjs_wrapGeom(Tendon, TCHAR_TO_UTF8(*Wrap.TargetName), SideSiteStr);
                if (!W)
                {
                    UE_LOG(LogURLabImport, Warning, TEXT("[MjTendon] mjs_wrapGeom failed for geom '%s' in tendon '%s'"),
                        *Wrap.TargetName, *TName);
                }
            }
            break;

        case EMjTendonWrapType::Pulley:
            {
                mjsWrap* W = mjs_wrapPulley(Tendon, (double)Wrap.Divisor);
                if (!W)
                {
                    UE_LOG(LogURLabImport, Warning, TEXT("[MjTendon] mjs_wrapPulley failed in tendon '%s'"), *TName);
                }
            }
            break;
        }
    }

    UE_LOG(LogURLabImport, Log, TEXT("[MjTendon] Registered '%s' with %d wraps"), *TName, Wraps.Num());
}

void UMjTendon::Bind(mjModel* model, mjData* data, const FString& Prefix)
{
    Super::Bind(model, data, Prefix);
    m_TendonView = BindToView<TendonView>(Prefix);
    if (m_TendonView.id != -1)
    {
        m_ID = m_TendonView.id;
    }
    else
    {
        UE_LOG(LogURLabImport, Warning, TEXT("[MjTendon] Failed to bind tendon '%s' (prefix='%s')"),
            *GetName(), *Prefix);
    }
}

float UMjTendon::GetLength() const
{
    return m_TendonView.GetLength();
}

float UMjTendon::GetVelocity() const
{
    return m_TendonView.GetVelocity();
}

FString UMjTendon::GetMjName() const
{
    if (m_TendonView.id < 0 || !m_TendonView.name) return FString();
    return MjUtils::MjToString(m_TendonView.name);
}

#if WITH_EDITOR
TArray<FString> UMjTendon::GetDefaultClassOptions() const
{
    return GetSiblingComponentOptions(this, UMjDefault::StaticClass(), true);
}
#endif
