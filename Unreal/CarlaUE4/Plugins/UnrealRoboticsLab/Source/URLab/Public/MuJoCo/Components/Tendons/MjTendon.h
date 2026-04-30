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
#include "mujoco/mujoco.h"
#include "mujoco/mjspec.h"
#include "MuJoCo/Utils/MjBind.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MjTendon.generated.h"


/**
 * @enum EMjTendonWrapType
 * @brief Defines the type of each wrap entry within a tendon.
 */
UENUM(BlueprintType)
enum class EMjTendonWrapType : uint8
{
    Joint   UMETA(DisplayName = "Joint (Fixed)"),
    Site    UMETA(DisplayName = "Site (Spatial)"),
    Geom    UMETA(DisplayName = "Geom (Spatial)"),
    Pulley  UMETA(DisplayName = "Pulley (Spatial)")
};


/**
 * @struct FMjTendonWrap
 * @brief Represents a single wrap entry in a MuJoCo tendon.
 *
 * A tendon is composed of an ordered list of wraps. For fixed tendons, use Joint wraps.
 * For spatial tendons, use Site, Geom, and Pulley wraps.
 */
USTRUCT(BlueprintType)
struct FMjTendonWrap
{
    GENERATED_BODY()

    /** @brief Type of this wrap entry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Wrap")
    EMjTendonWrapType Type = EMjTendonWrapType::Joint;

    /**
     * @brief Name of the target joint, site, or geom.
     * Ignored for Pulley wraps.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Wrap")
    FString TargetName;

    /**
     * @brief Gear coefficient for Joint wraps (how much of joint movement maps to tendon length).
     * Ignored for Site, Geom, Pulley wraps.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Wrap")
    float Coef = 1.0f;

    /**
     * @brief Name of the side-site for Geom wraps.
     * The side-site disambiguates which side of the cylinder to wrap around.
     * Ignored for Joint, Site, Pulley wraps.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Wrap")
    FString SideSite;

    /**
     * @brief The pulley divisor for Pulley wraps.
     * Divides the tendon's effective length change; use for compound pulleys.
     * Ignored for Joint, Site, Geom wraps.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Wrap")
    float Divisor = 1.0f;
};


/**
 * @class UMjTendon
 * @brief Component representing a MuJoCo tendon.
 *
 * Tendons are kinematic coupling mechanisms that can:
 * - Connect joints (fixed tendon: uses Joint wraps with gear coefficients)
 * - Route through space (spatial tendon: uses Site, Geom, and Pulley wraps)
 *
 * This component mirrors the `<fixed>` and `<spatial>` elements inside `<tendon>` in MuJoCo XML.
 * Attach this component directly to the AMjArticulation root (not to a body).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjTendon : public UMjComponent
{
    GENERATED_BODY()

public:
    UMjTendon();

    /**
     * @brief Exports properties to a pre-created MuJoCo spec tendon structure.
     * @param Tendon Pointer to the target mjsTendon structure.
     * @param def Optional default structure for optimized export.
     */
    void ExportTo(mjsTendon* Tendon, mjsDefault* def = nullptr);

    /**
     * @brief Registers this tendon to the MuJoCo spec.
     * Calls mjs_addTendon, ExportTo, then mjs_wrapJoint/Site/Geom/Pulley for each Wrap entry.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody Unused for tendons (tendons are spec-level, not body-children).
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    /**
     * @brief Binds this component to the live MuJoCo simulation.
     * Resolves the tendon ID by prefixed name and populates m_TendonView.
     */
    virtual void Bind(mjModel* model, mjData* data, const FString& Prefix = TEXT("")) override;

    /**
     * @brief Imports properties from a raw XML node.
     * @param Node The <fixed> or <spatial> XML child node inside <tendon>.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /** @brief The runtime view of the MuJoCo tendon. Valid only after Bind() is called. */
    TendonView m_TendonView;

    /** @brief Semantic accessor for raw MuJoCo data and helper methods. */
    TendonView& GetMj() { return m_TendonView; }
    const TendonView& GetMj() const { return m_TendonView; }

    // --- Runtime Accessors ---

    /**
     * @brief Gets the current tendon length (meters).
     * Valid only for 1D tendons after Bind(). Returns 0 if not bound.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetLength() const;

    /**
     * @brief Gets the current tendon velocity (m/s).
     * Returns 0 if not bound.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetVelocity() const;

    /** @brief Gets the full prefixed name of this tendon in the compiled MuJoCo model. */
    virtual FString GetMjName() const override;

    // --- Wrap Entries ---

    /**
     * @brief Ordered list of wrap entries that define the tendon path.
     * For fixed tendons: add Joint wraps.
     * For spatial tendons: add Site, Geom, and/or Pulley wraps.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon")
    TArray<FMjTendonWrap> Wraps;

    /** @brief Optional MuJoCo default class name to inherit from (string fallback). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon", meta=(GetOptions="GetDefaultClassOptions"))
    FString MjClassName;

    /** @brief Reference to a UMjDefault component for default class inheritance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon")
    UMjDefault* DefaultClass = nullptr;

    virtual FString GetMjClassName() const override
    {
        return MjClassName;
    }

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetDefaultClassOptions() const;
#endif

    // --- Physics Properties ---

    /** @brief Override toggle for Stiffness. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Stiffness = false;

    /** @brief Tendon stiffness coefficients [linear, poly0, poly1]. Polynomial spring: F = -x*(linear + poly0*x + poly1*x²). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Physics", meta=(EditCondition="bOverride_Stiffness"))
    TArray<float> Stiffness = {0.0f};

    /** @brief Override toggle for SpringLength. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_SpringLength = false;

    /**
     * @brief Spring resting length [min, max]. Use (-1, -1) to compute from qpos_spring.
     * For fixed tendons this is usually a single value; store in X, leave Y as -1.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Physics", meta=(EditCondition="bOverride_SpringLength"))
    TArray<float> SpringLength;

    /** @brief Override toggle for Damping. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Damping = false;

    /** @brief Damping coefficients [linear, poly0, poly1]. Polynomial damper: F = -v*(linear + poly0*|v| + poly1*v²). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Physics", meta=(EditCondition="bOverride_Damping"))
    TArray<float> Damping = {0.0f};

    /** @brief Override toggle for FrictionLoss. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_FrictionLoss = false;

    /** @brief Friction loss (dry friction applied along the tendon). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Physics", meta=(EditCondition="bOverride_FrictionLoss"))
    float FrictionLoss = 0.0f;

    /** @brief Override toggle for Armature. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Armature = false;

    /** @brief Inertia associated with the tendon velocity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Physics", meta=(EditCondition="bOverride_Armature"))
    float Armature = 0.0f;

    // --- Limits ---

    /** @brief Override toggle for bLimited. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Limits", meta=(InlineEditConditionToggle))
    bool bOverride_Limited = false;

    /** @brief Whether the tendon has length limits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Limits", meta=(EditCondition="bOverride_Limited"))
    bool bLimited = false;

    /** @brief Override toggle for Range. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Limits", meta=(InlineEditConditionToggle))
    bool bOverride_Range = false;

    /** @brief Tendon length limits [min, max]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Limits", meta=(EditCondition="bOverride_Range"))
    TArray<float> Range = {0.0f, 0.0f};

    /** @brief Override toggle for Margin. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Limits", meta=(InlineEditConditionToggle))
    bool bOverride_Margin = false;

    /** @brief Margin for limit detection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Limits", meta=(EditCondition="bOverride_Margin"))
    float Margin = 0.0f;

    // --- Actuator Limits ---

    /** @brief Override toggle for bActFrcLimited. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Limits", meta=(InlineEditConditionToggle))
    bool bOverride_ActFrcLimited = false;

    /** @brief Whether the tendon has actuator force limits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Limits", meta=(EditCondition="bOverride_ActFrcLimited"))
    bool bActFrcLimited = false;

    /** @brief Override toggle for ActFrcRange. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Limits", meta=(InlineEditConditionToggle))
    bool bOverride_ActFrcRange = false;

    /** @brief Tendon actuator force limits [min, max]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Limits", meta=(EditCondition="bOverride_ActFrcRange"))
    TArray<float> ActFrcRange = {0.0f, 0.0f};

    // --- Solver Parameters ---

    /** @brief Override toggle for SolRefLimit. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Solving", meta=(InlineEditConditionToggle))
    bool bOverride_SolRefLimit = false;

    /** @brief Constraint solver reference for limits (timeconst, dampratio). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Solving", meta=(EditCondition="bOverride_SolRefLimit"))
    TArray<float> SolRefLimit = {0.02f, 1.0f};

    /** @brief Override toggle for SolImpLimit. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Solving", meta=(InlineEditConditionToggle))
    bool bOverride_SolImpLimit = false;

    /** @brief Constraint solver impedance for limits (dmin, dmax, width). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Solving", meta=(EditCondition="bOverride_SolImpLimit"))
    TArray<float> SolImpLimit = {0.9f, 0.95f, 0.001f};

    /** @brief Override toggle for SolRefFriction. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Solving", meta=(InlineEditConditionToggle))
    bool bOverride_SolRefFriction = false;

    /** @brief Constraint solver reference for friction (timeconst, dampratio). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Solving", meta=(EditCondition="bOverride_SolRefFriction"))
    TArray<float> SolRefFriction = {0.02f, 1.0f};

    /** @brief Override toggle for SolImpFriction. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Solving", meta=(InlineEditConditionToggle))
    bool bOverride_SolImpFriction = false;

    /** @brief Constraint solver impedance for friction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Solving", meta=(EditCondition="bOverride_SolImpFriction"))
    TArray<float> SolImpFriction = {0.9f, 0.95f, 0.001f};

    // --- Visuals ---

    /** @brief Override toggle for Width. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Visual", meta=(InlineEditConditionToggle))
    bool bOverride_Width = false;

    /** @brief Width for rendering the tendon (meters). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Visual", meta=(EditCondition="bOverride_Width"))
    float Width = 0.003f;

    /** @brief Override toggle for Rgba. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Visual", meta=(InlineEditConditionToggle))
    bool bOverride_Rgba = false;

    /** @brief Tendon color (RGBA). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Visual", meta=(EditCondition="bOverride_Rgba"))
    FLinearColor Rgba = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

    /** @brief Override toggle for Group. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Tendon|Visual", meta=(InlineEditConditionToggle))
    bool bOverride_Group = false;

    /** @brief Visualization group (0 = always visible). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Tendon|Visual", meta=(EditCondition="bOverride_Group"))
    int Group = 0;

protected:
    virtual void BeginPlay() override;
};
