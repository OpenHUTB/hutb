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
#include "Components/SceneComponent.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MjSite.generated.h"

/**
 * @enum EMjSiteType
 * @brief Defines the geometric shape of a MuJoCo site.
 */
UENUM(BlueprintType)
enum class EMjSiteType : uint8
{
	Sphere		UMETA(DisplayName = "Sphere"),
	Capsule		UMETA(DisplayName = "Capsule"),
	Ellipsoid	UMETA(DisplayName = "Ellipsoid"),
	Cylinder	UMETA(DisplayName = "Cylinder"),
	Box			UMETA(DisplayName = "Box"),
};

/**
 * @class UMjSite
 * @brief Represents a MuJoCo site element within Unreal Engine.
 * 
 * Sites are interesting locations on a body, used for sensor attachment, constraint definition, or visualization.
 * This component mirrors the `site` element in MuJoCo XML.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjSite : public UMjComponent
{
	GENERATED_BODY()



public:

    /**
     * @brief Exports properties to a MuJoCo spec site structure.
     * @param site Pointer to the target mjsSite structure.
     * @param def Optional default structure for optimized export.
     */
    void ExportTo(mjsSite* site, mjsDefault* def = nullptr);
    
    /**
     * @brief Binds this component to the live MuJoCo simulation.
     */
    virtual void Bind(mjModel* model, mjData* data, const FString& Prefix = TEXT("")) override;

    /**
     * @brief Registers this site to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody The parent body to attach to.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody) override;

    /**
     * @brief Imports properties from a MuJoCo XML node.
     * @param Node Pointer to the XML node.
     */
    void ImportFromXml(const class FXmlNode* Node);
    void ImportFromXml(const class FXmlNode* Node, const struct FMjCompilerSettings& CompilerSettings);

public:	
    /** @brief Default constructor. */
	UMjSite();

public:	
	// --- Geometric Properties ---
    /** @brief The geometric shape type of the site. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site")
	EMjSiteType Type = EMjSiteType::Sphere;

	/** @brief Dimensions of the site geometry. Interpretation depends on Type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site")
	FVector Size = FVector(0.01f); 

    /** @brief Optional MuJoCo class name to inherit defaults from (string fallback). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site", meta=(GetOptions="GetDefaultClassOptions"))
    FString MjClassName;

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetDefaultClassOptions() const;
#endif

    /** @brief Reference to a UMjDefault component for default class inheritance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site")
    UMjDefault* DefaultClass = nullptr;

    virtual FString GetMjClassName() const override
    {
        return MjClassName;
    }

	// --- Visuals ---
    /** @brief RGBA Color of the site visualization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site")
	FLinearColor Rgba = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
    
    /** @brief Integer group ID for visibility filtering. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site")
    int32 Group = 0;

    // --- FromTo Overrides ---
    /** @brief Override toggle for FromTo. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Site", meta=(InlineEditConditionToggle))
    bool bOverride_FromTo = false;

    /** @brief Start point for fromto definition. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site", meta=(EditCondition="bOverride_FromTo"))
    FVector FromToStart = FVector(0.0f); 

    /** @brief End point for fromto definition. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Site", meta=(EditCondition="bOverride_FromTo"))
    FVector FromToEnd = FVector(0.0f, 0.0f, 1.0f);

    /** @brief The runtime view of the MuJoCo site. Valid only after Bind() is called. */
    SiteView m_SiteView;

    /** @brief Semantic accessor for raw MuJoCo data and helper methods. */
    SiteView& GetMj() { return m_SiteView; }
    const SiteView& GetMj() const { return m_SiteView; }
};
