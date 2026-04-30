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

#include "MjComponentDetailCustomizations.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Physics/MjContactPair.h"
#include "MuJoCo/Components/Physics/MjContactExclude.h"
#include "MuJoCo/Components/Constraints/MjEquality.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"

// Detail customizations that hide internal properties from the editor UI.

// ============================================================================
// Actuator — hide DefaultClass pointer
// ============================================================================

TSharedRef<IDetailCustomization> FMjActuatorDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjActuatorDetailCustomization);
}

void FMjActuatorDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjActuator, DefaultClass)));
}

// ============================================================================
// Sensor — hide DefaultClass pointer
// ============================================================================

TSharedRef<IDetailCustomization> FMjSensorDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjSensorDetailCustomization);
}

void FMjSensorDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjSensor, DefaultClass)));
}

// ============================================================================
// Contact Pair — hide Name
// ============================================================================

TSharedRef<IDetailCustomization> FMjContactPairDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjContactPairDetailCustomization);
}

void FMjContactPairDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjContactPair, Name)));
}

// ============================================================================
// Contact Exclude — hide Name
// ============================================================================

TSharedRef<IDetailCustomization> FMjContactExcludeDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjContactExcludeDetailCustomization);
}

void FMjContactExcludeDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjContactExclude, Name)));
}

// ============================================================================
// Equality
// ============================================================================

TSharedRef<IDetailCustomization> FMjEqualityDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjEqualityDetailCustomization);
}

void FMjEqualityDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}

// ============================================================================
// Joint — hide DefaultClass pointer
// ============================================================================

TSharedRef<IDetailCustomization> FMjJointDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjJointDetailCustomization);
}

void FMjJointDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjJoint, DefaultClass)));
}

// ============================================================================
// Body
// ============================================================================

TSharedRef<IDetailCustomization> FMjBodyDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjBodyDetailCustomization);
}

void FMjBodyDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}

// ============================================================================
// Default — hide ClassName and ParentClassName (synced from SCS hierarchy)
// ============================================================================

TSharedRef<IDetailCustomization> FMjDefaultDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjDefaultDetailCustomization);
}

void FMjDefaultDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjDefault, ClassName)));
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjDefault, ParentClassName)));
}

// ============================================================================
// Site — hide DefaultClass pointer
// ============================================================================

TSharedRef<IDetailCustomization> FMjSiteDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjSiteDetailCustomization);
}

void FMjSiteDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjSite, DefaultClass)));
}

// ============================================================================
// Tendon — hide DefaultClass pointer
// ============================================================================

TSharedRef<IDetailCustomization> FMjTendonDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjTendonDetailCustomization);
}

void FMjTendonDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjTendon, DefaultClass)));
}

// ============================================================================
// Geom — hide DefaultClass pointer + CoACD decomposition buttons
// ============================================================================

TSharedRef<IDetailCustomization> FMjGeomDetailCustomization::MakeInstance()
{
    return MakeShareable(new FMjGeomDetailCustomization);
}

void FMjGeomDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);

    if (Objects.Num() != 1) return;
    TWeakObjectPtr<UMjGeom> WeakGeom = Cast<UMjGeom>(Objects[0].Get());
    if (!WeakGeom.IsValid()) return;

    DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMjGeom, DefaultClass)));

    // Decomposition buttons (only for mesh geoms)
    if (WeakGeom->Type != EMjGeomType::Mesh) return;

    IDetailCategoryBuilder& DecompCategory = DetailBuilder.EditCategory("MuJoCo|Geom|Decomposition");

    DecompCategory.AddCustomRow(FText::FromString("Decompose"))
        .NameContent()
        [
            SNew(STextBlock).Text(FText::FromString("CoACD Decomposition"))
        ]
        .ValueContent()
        .MaxDesiredWidth(300.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.f)
            [
                SNew(SButton)
                .Text(FText::FromString("Decompose Mesh"))
                .OnClicked_Lambda([WeakGeom]() -> FReply
                {
                    if (WeakGeom.IsValid()) WeakGeom->DecomposeMesh();
                    return FReply::Handled();
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.f)
            [
                SNew(SButton)
                .Text(FText::FromString("Remove Decomposition"))
                .OnClicked_Lambda([WeakGeom]() -> FReply
                {
                    if (WeakGeom.IsValid()) WeakGeom->RemoveDecomposition();
                    return FReply::Handled();
                })
            ]
        ];
}
