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
#include "Blueprint/UserWidget.h"
#include "MjPropertyRow.generated.h"

class UTextBlock;
class USlider;
class UCheckBox;

/**
 * @enum EMjPropertyType
 * @brief Defines the type of control displayed by the property row.
 */
UENUM(BlueprintType)
enum class EMjPropertyType : uint8
{
    Slider,
    Toggle,
    LabelOnly,
    Header
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMjPropertyValueChanged, float, NewValue, const FString&, PropertyID);

/**
 * @class UMjPropertyRow
 * @brief A reusable widget base for a single property row (Label + Control + ValueDisplay).
 * 
 * In the Blueprint, you should bind:
 * - PropertyLabel (TextBlock)
 * - PropertySlider (Slider) [Optional for Toggle mode]
 * - PropertyToggle (CheckBox) [Optional for Slider mode]
 * - ValueDisplay (TextBlock)
 */
UCLASS()
class URLAB_API UMjPropertyRow : public UUserWidget
{
    GENERATED_BODY()

public:
    /** @brief Sets up the row with the given property name and type. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void InitializeProperty(const FString& InName, EMjPropertyType InType, float InitialValue, FVector2D Range = FVector2D(0.0f, 1.0f), const FString& InDisplayName = TEXT(""));

    /** @brief Manually sets the slider range. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void SetSliderRange(FVector2D Range);

    /** @brief Updates the displayed value without triggering events. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void SetValue(float NewValue);

    /** @brief Event fired when the value changes via user interaction. */
    UPROPERTY(BlueprintAssignable, Category = "MuJoCo|UI")
    FMjPropertyValueChanged OnValueChanged;

    /** @brief Gets the name of the property this row represents. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|UI")
    FString GetPropertyName() const { return PropertyName; }

    /** @brief Gets the type of the property this row represents. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|UI")
    EMjPropertyType GetPropertyType() const { return PropertyType; }

    /** @brief Sets whether this row is an interactive control (actuator). */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void SetControllable(bool bInControllable) { bIsControllable = bInControllable; }

    /** @brief Returns true if this is an interactive actuator control. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|UI")
    bool IsControllable() const { return bIsControllable; }

    /** @brief Checks if the user is currently dragging the slider. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|UI")
    bool IsBeingDragged() const;

    /** @brief Enables or disables the interactive widgets in this row. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void SetRowEnabled(bool bEnabled);

    /** @brief Sets the object (Component or otherwise) associated with this row. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void SetAssociatedObject(UObject* InObject) { AssociatedObject = InObject; }

    /** @brief Gets the object associated with this row. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|UI")
    UObject* GetAssociatedObject() const { return AssociatedObject.Get(); }

protected:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* PropertyLabel;

    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    USlider* PropertySlider;

    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UCheckBox* PropertyToggle;

    UPROPERTY(meta = (BindWidget, OptionalWidget = true))
    UTextBlock* ValueDisplay;

    UPROPERTY(BlueprintReadOnly, Category = "MuJoCo|UI")
    FString PropertyName;

    UPROPERTY(BlueprintReadOnly, Category = "MuJoCo|UI")
    EMjPropertyType PropertyType;

    UPROPERTY(BlueprintReadOnly, Category = "MuJoCo|UI")
    bool bIsControllable = false;

    TWeakObjectPtr<UObject> AssociatedObject;

    UFUNCTION()
    void HandleSliderChanged(float Value);

    UFUNCTION()
    void HandleToggleChanged(bool bChecked);

    virtual void NativeConstruct() override;
};
