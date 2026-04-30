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
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "MjFrame.generated.h"

/**
 * @class UMjFrame
 * @brief Represents a MuJoCo frame (mjsFrame) in the spec.
 * 
 * Frames are used for coordinate transformations and are compiled away 
 * by the MuJoCo compiler into relative offsets for attached elements.
 * This component does not support runtime binding.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjFrame : public UMjComponent
{
    GENERATED_BODY()

public:
    UMjFrame();

    /**
     * @brief Registers this frame to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody The parent MuJoCo body.
     */
    virtual void Setup(USceneComponent* Parent, mjsBody* ParentBody, class FMujocoSpecWrapper* Wrapper);

    /** @brief Empty implementation as frames are compiled away and don't exist at runtime in mjModel/mjData. */
    virtual void Bind(mjModel* Model, mjData* Data, const FString& Prefix = TEXT("")) override;

    /** @brief Frames are not used in SpecElement flow directly but via Setup. */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    /**
     * @brief Imports pos/orientation from an XML <frame> node.
     * Sets the component's RelativeLocation and RelativeRotation in UE space.
     * @param Node  The <frame> XML element.
     * @param CompilerSettings  Angle units and euler sequence from <compiler>.
     */
    void ImportFromXml(const class FXmlNode* Node, const FMjCompilerSettings& CompilerSettings = FMjCompilerSettings{});

private:
    /** @brief Cached list of child elements for registration. */
    UPROPERTY()
    TArray<TScriptInterface<IMjSpecElement>> m_SpecElements;
};
