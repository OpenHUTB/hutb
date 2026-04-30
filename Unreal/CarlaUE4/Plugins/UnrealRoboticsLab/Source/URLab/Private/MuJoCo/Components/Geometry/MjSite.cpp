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

#include "MuJoCo/Components/Geometry/MjSite.h"
#include "mujoco/mujoco.h"
#include "XmlFile.h"

#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "Utils/URLabLogging.h"

UMjSite::UMjSite()
{
	PrimaryComponentTick.bCanEverTick = false;
    
    Type = EMjSiteType::Sphere;
    Size = FVector(0.01f);
    Rgba = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
    Group = 0;
}

void UMjSite::ExportTo(mjsSite* site, mjsDefault* def)
{
    if (!site) return;
    UE_LOG(LogURLabExport, Log, TEXT("Exporting site %s"), *this->GetName());
    
    // Type
    switch(Type)
    {
        case EMjSiteType::Sphere: site->type = mjGEOM_SPHERE; break;
        case EMjSiteType::Capsule: site->type = mjGEOM_CAPSULE; break;
        case EMjSiteType::Ellipsoid: site->type = mjGEOM_ELLIPSOID; break;
        case EMjSiteType::Cylinder: site->type = mjGEOM_CYLINDER; break;
        case EMjSiteType::Box: site->type = mjGEOM_BOX; break;
    }

    // Size
    site->size[0] = Size.X;
    site->size[1] = Size.Y;
    site->size[2] = Size.Z;

    // Transform
    FTransform RelTrans = GetRelativeTransform();
    
    MjUtils::UEToMjPosition(RelTrans.GetLocation(), site->pos);
    MjUtils::UEToMjRotation(RelTrans.GetRotation(), site->quat);
    
    // Override FromTo
    if (bOverride_FromTo)
    {
        double Start[3], End[3];
        MjUtils::UEToMjPosition(FromToStart, Start);
        MjUtils::UEToMjPosition(FromToEnd, End);
        
        // Write to site->fromto (assuming mjsSite has 'fromto')
        site->fromto[0] = Start[0]; site->fromto[1] = Start[1]; site->fromto[2] = Start[2];
        site->fromto[3] = End[0];   site->fromto[4] = End[1];   site->fromto[5] = End[2];
        
        // Zero out pos/quat to prioritize fromto
        for(int i=0; i<3; ++i) site->pos[i] = 0.0;
        site->quat[0] = 1.0; site->quat[1] = 0.0; site->quat[2] = 0.0; site->quat[3] = 0.0;
    }

    // Visuals & Group (Conditional)
    if (!def || Group != def->site->group)
        site->group = Group;

    if (!def ||
        Rgba.R != def->site->rgba[0] ||
        Rgba.G != def->site->rgba[1] ||
        Rgba.B != def->site->rgba[2] ||
        Rgba.A != def->site->rgba[3])
    {
        site->rgba[0] = Rgba.R;
        site->rgba[1] = Rgba.G;
        site->rgba[2] = Rgba.B;
        site->rgba[3] = Rgba.A;
    }
}

void UMjSite::ImportFromXml(const FXmlNode* Node)
{
    FMjCompilerSettings DefaultSettings;
    ImportFromXml(Node, DefaultSettings);
}

void UMjSite::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;

    // Type
    FString TypeStr = Node->GetAttribute(TEXT("type"));
    if (TypeStr == "sphere") Type = EMjSiteType::Sphere;
    else if (TypeStr == "capsule") Type = EMjSiteType::Capsule;
    else if (TypeStr == "ellipsoid") Type = EMjSiteType::Ellipsoid;
    else if (TypeStr == "cylinder") Type = EMjSiteType::Cylinder;
    else if (TypeStr == "box") Type = EMjSiteType::Box;

    // Size
    {
        TArray<float> SizeArr;
        bool bSizeOverride = false;
        if (MjXmlUtils::ReadAttrFloatArray(Node, TEXT("size"), SizeArr, bSizeOverride))
        {
            if (SizeArr.Num() >= 3)
                Size = FVector(SizeArr[0], SizeArr[1], SizeArr[2]);
            else if (SizeArr.Num() == 1)
                Size = FVector(SizeArr[0]);
        }
    }

    // RGBA
    {
        TArray<float> RgbaArr;
        bool bRgbaOverride = false;
        if (MjXmlUtils::ReadAttrFloatArray(Node, TEXT("rgba"), RgbaArr, bRgbaOverride) && RgbaArr.Num() >= 4)
            Rgba = FLinearColor(RgbaArr[0], RgbaArr[1], RgbaArr[2], RgbaArr[3]);
    }

    // Transform - Position
    FString PosStr = Node->GetAttribute(TEXT("pos"));
    if (!PosStr.IsEmpty())
    {
        TArray<FString> Parts;
        PosStr.ParseIntoArray(Parts, TEXT(" "), true);
        if (Parts.Num() >= 3)
        {
            double pos[3] = { FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1]), FCString::Atod(*Parts[2]) };
            SetRelativeLocation(MjUtils::MjToUEPosition(pos));
        }
    }
    
    // Orientation (quat, axisangle, euler, xyaxes, zaxis — priority order)
    double MjQuat[4];
    if (MjOrientationUtils::OrientationToMjQuat(Node, CompilerSettings, MjQuat))
    {
        SetRelativeRotation(MjUtils::MjToUERotation(MjQuat));
    }

    // Group
    bool bOverride_Group = false;
    MjXmlUtils::ReadAttrInt(Node, TEXT("group"), Group, bOverride_Group);

    // FromTo (Site can be cylinders etc) — resolve to pos/quat/size
    FString FromToStr = Node->GetAttribute(TEXT("fromto"));
    if (!FromToStr.IsEmpty())
    {
         if (MjUtils::ParseFromTo(FromToStr, FromToStart, FromToEnd))
         {
             // Position = midpoint
             FVector Midpoint = (FromToStart + FromToEnd) * 0.5f;
             SetRelativeLocation(Midpoint);

             // Rotation = align local +Z with fromto direction
             FVector Dir = (FromToEnd - FromToStart).GetSafeNormal();
             if (!Dir.IsNearlyZero())
             {
                 FQuat Rot = FQuat::FindBetweenNormals(FVector(0.f, 0.f, 1.f), Dir);
                 SetRelativeRotation(Rot);
             }

             // Half-length from fromto distance (UE cm -> MuJoCo m)
             float HalfLength = (FromToEnd - FromToStart).Size() * 0.5f / 100.0f;

             // Site types follow same convention as geoms
             if (Type == EMjSiteType::Box || Type == EMjSiteType::Ellipsoid)
             {
                 Size.Z = HalfLength;
             }
             else
             {
                 // Capsule, Cylinder, Sphere
                 Size.Y = HalfLength;
             }

             // Resolved — don't pass raw fromto to spec
             bOverride_FromTo = false;

             UE_LOG(LogURLabImport, Log, TEXT("[MjSite::ImportFromXml] '%s' FromTo resolved: pos=%s, halfLen=%.4f m"),
                *GetName(), *Midpoint.ToString(), HalfLength);
         }
    }
    
}

void UMjSite::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    if (!ParentBody) return;

    mjsDefault* effectiveDefault = ResolveDefault(Wrapper.Spec, MjClassName);

    mjsSite* site = mjs_addSite(ParentBody, effectiveDefault);
    m_SpecElement = site->element;
    SetSpecElementName(Wrapper, site->element, mjOBJ_SITE);

    ExportTo(site, effectiveDefault);
}

void UMjSite::Bind(mjModel* model, mjData* data, const FString& Prefix)
{
    Super::Bind(model, data, Prefix);
    m_SiteView = BindToView<SiteView>(Prefix);

    if (m_SiteView.id != -1)
    {
        m_ID = m_SiteView.id;
        UE_LOG(LogURLabBind, Log, TEXT("[MjSite] Successfully bound '%s' to ID %d (MjName: %s)"), *GetName(), m_ID, *MjName);
    }
    else
    {
        UE_LOG(LogURLabBind, Warning, TEXT("[MjSite] Site '%s' FAILED bind. Prefix: %s, MjName: %s"), *GetName(), *Prefix, *MjName);
    }
}

#if WITH_EDITOR
TArray<FString> UMjSite::GetDefaultClassOptions() const
{
    return GetSiblingComponentOptions(this, UMjDefault::StaticClass(), true);
}
#endif
