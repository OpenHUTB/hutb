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

#include "MjEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/AppStyle.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FMjEditorStyle::StyleInstance = nullptr;

FName FMjEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("MjEditorStyle"));
	return StyleSetName;
}

void FMjEditorStyle::Initialize()
{
	if (StyleInstance.IsValid()) return;

	StyleInstance = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	// Resolve the plugin's Resources directory for custom icon PNGs
	FString PluginBaseDir = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("UnrealRoboticsLab"));
	FString IconsDir = PluginBaseDir / TEXT("Resources") / TEXT("Icons");

	StyleInstance->SetContentRoot(IconsDir);

	// Icon size for component tree
	const FVector2D IconSize(16.0f, 16.0f);

	// Helper lambda: register icon if PNG exists, otherwise use an existing engine brush
	auto RegisterIcon = [&](const FName& PropertyName, const FString& PngName, const FName& FallbackBrush)
	{
		FString PngPath = IconsDir / PngName;
		if (FPaths::FileExists(PngPath))
		{
			StyleInstance->Set(PropertyName, new FSlateImageBrush(PngPath, IconSize));
		}
		else
		{
			// Copy the existing brush from the engine's style set
			const FSlateBrush* Existing = FAppStyle::Get().GetBrush(FallbackBrush);
			if (Existing && Existing != FAppStyle::GetNoBrush())
			{
				StyleInstance->Set(PropertyName, new FSlateBrush(*Existing));
			}
		}
	};

	// Register class icons for each MuJoCo component type.
	// UE looks for "ClassIcon.<ClassName>" in registered style sets.
	// If custom PNGs are placed in Resources/Icons/, they will be used.
	// Otherwise, built-in editor icons serve as placeholders.
	RegisterIcon("ClassIcon.MjBody",            TEXT("MjBody.png"),            "ClassIcon.SceneComponent");
	RegisterIcon("ClassIcon.MjWorldBody",       TEXT("MjWorldBody.png"),       "ClassIcon.SceneComponent");
	RegisterIcon("ClassIcon.MjGeom",            TEXT("MjGeom.png"),            "ClassIcon.StaticMeshComponent");
	RegisterIcon("ClassIcon.MjBox",             TEXT("MjBox.png"),             "ClassIcon.StaticMeshComponent");
	RegisterIcon("ClassIcon.MjSphere",          TEXT("MjSphere.png"),          "ClassIcon.StaticMeshComponent");
	RegisterIcon("ClassIcon.MjCylinder",        TEXT("MjCylinder.png"),        "ClassIcon.StaticMeshComponent");
	RegisterIcon("ClassIcon.MjCapsule",         TEXT("MjCapsule.png"),         "ClassIcon.StaticMeshComponent");
	RegisterIcon("ClassIcon.MjMeshGeom",        TEXT("MjMeshGeom.png"),        "ClassIcon.StaticMeshComponent");
	RegisterIcon("ClassIcon.MjFlexcomp",        TEXT("MjFlexcomp.png"),        "ClassIcon.SkeletalMeshComponent");
	RegisterIcon("ClassIcon.MjHingeJoint",      TEXT("MjHingeJoint.png"),      "ClassIcon.PhysicsConstraintComponent");
	RegisterIcon("ClassIcon.MjSlideJoint",      TEXT("MjSlideJoint.png"),      "ClassIcon.PhysicsConstraintComponent");
	RegisterIcon("ClassIcon.MjBallJoint",       TEXT("MjBallJoint.png"),       "ClassIcon.PhysicsConstraintComponent");
	RegisterIcon("ClassIcon.MjFreeJoint",       TEXT("MjFreeJoint.png"),       "ClassIcon.PhysicsConstraintComponent");
	RegisterIcon("ClassIcon.MjJoint",           TEXT("MjJoint.png"),           "ClassIcon.PhysicsConstraintComponent");
	RegisterIcon("ClassIcon.MjSensor",          TEXT("MjSensor.png"),          "ClassIcon.SphereComponent");
	RegisterIcon("ClassIcon.MjActuator",        TEXT("MjActuator.png"),        "ClassIcon.MovementComponent");
	RegisterIcon("ClassIcon.MjDefault",         TEXT("MjDefault.png"),         "ClassIcon.BlueprintCore");
	RegisterIcon("ClassIcon.MjSite",            TEXT("MjSite.png"),            "ClassIcon.TargetPoint");
	RegisterIcon("ClassIcon.MjTendon",          TEXT("MjTendon.png"),          "ClassIcon.CableComponent");
	RegisterIcon("ClassIcon.MjCamera",          TEXT("MjCamera.png"),          "ClassIcon.CameraComponent");
	RegisterIcon("ClassIcon.MjInertial",        TEXT("MjInertial.png"),        "ClassIcon.SphereComponent");
	RegisterIcon("ClassIcon.MjContactPair",     TEXT("MjContactPair.png"),     "ClassIcon.SphereComponent");
	RegisterIcon("ClassIcon.MjContactExclude",  TEXT("MjContactExclude.png"),  "ClassIcon.SphereComponent");
	RegisterIcon("ClassIcon.MjEquality",        TEXT("MjEquality.png"),        "ClassIcon.SphereComponent");
	RegisterIcon("ClassIcon.MjKeyframe",        TEXT("MjKeyframe.png"),        "ClassIcon.SphereComponent");

	FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
}

void FMjEditorStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}
}

const ISlateStyle& FMjEditorStyle::Get()
{
	check(StyleInstance.IsValid());
	return *StyleInstance;
}
