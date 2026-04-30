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

#include <mujoco/mjspec.h>

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "MuJoCo/Core/Spec/MjSpecElement.h"
#include "MjContactExclude.generated.h"

/**
 * @class UMjContactExclude
 * @brief Component representing a MuJoCo contact exclusion.
 * 
 * Prevents collision checking between all geoms of two bodies.
 * Corresponds to the <exclude> element in MuJoCo XML.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjContactExclude : public USceneComponent, public IMjSpecElement
{
	GENERATED_BODY()

public:	
    /** @brief Default constructor. */
	UMjContactExclude();

    /** @brief Name of the contact exclusion (optional). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Exclude")
    FString Name;

    /** @brief Name of the first body in the exclusion pair (required). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Exclude", meta=(GetOptions="GetBodyOptions"))
    FString Body1;

    /** @brief Name of the second body in the exclusion pair (required). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Exclude", meta=(GetOptions="GetBodyOptions"))
    FString Body2;

    /**
     * @brief Imports contact exclusion settings from a MuJoCo XML <exclude> node.
     * @param Node Pointer to the FXmlNode representing the <exclude> element.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /**
     * @brief Exports contact exclusion settings to a MuJoCo mjsExclude structure.
     * @param exclude Pointer to the mjsExclude structure to populate.
     */

    void ExportTo(mjsExclude* exclude);

    /**
     * @brief Registers this contact exclusion to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    virtual void Bind(mjModel* model, mjData* data, const FString& Prefix = TEXT("")) override;

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetBodyOptions() const;
#endif

protected:
    /** @brief Called when the game starts. */
	virtual void BeginPlay() override;
};
