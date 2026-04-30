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
#include "MjContactPair.generated.h"

/**
 * @class UMjContactPair
 * @brief Component representing a MuJoCo contact pair.
 * 
 * Explicitly defines contact between two geoms with custom properties.
 * Corresponds to the <pair> element in MuJoCo XML.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjContactPair : public USceneComponent, public IMjSpecElement
{
	GENERATED_BODY()

public:	
    /** @brief Default constructor. */
	UMjContactPair();

    /** @brief Name of the contact pair (optional). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair")
    FString Name;

    /** @brief Name of the first geom in the contact pair (required). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair", meta=(GetOptions="GetGeomOptions"))
    FString Geom1;

    /** @brief Name of the second geom in the contact pair (required). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair", meta=(GetOptions="GetGeomOptions"))
    FString Geom2;

    /** @brief Contact dimensionality (1-6). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair")
    int Condim = 3;

    /** @brief Friction coefficients (sliding, torsional, rolling). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair")
    TArray<float> Friction;

    /** @brief Solver reference time/damping for contact. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair")
    TArray<float> Solref;

    /** @brief Solver impedance for contact. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair")
    TArray<float> Solimp;

    /** @brief Distance threshold below which contacts are detected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair")
    float Gap = 0.0f;

    /** @brief Safety margin for collision detection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Contact Pair")
    float Margin = 0.0f;

    /**
     * @brief Imports contact pair settings from a MuJoCo XML <pair> node.
     * @param Node Pointer to the FXmlNode representing the <pair> element.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /**
     * @brief Exports contact pair settings to a MuJoCo mjsPair structure.
     * @param pair Pointer to the mjsPair structure to populate.
     */

    void ExportTo(mjsPair* pair);

    /**
     * @brief Registers this contact pair to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    virtual void Bind(mjModel* model, mjData* data, const FString& Prefix = TEXT("")) override;

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetGeomOptions() const;
#endif

protected:
    /** @brief Called when the game starts. */
	virtual void BeginPlay() override;
};
