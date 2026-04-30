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

#include "UI/MjPropertyRow.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"

#include "Components/HorizontalBoxSlot.h"

void UMjPropertyRow::NativeConstruct()
{
    Super::NativeConstruct();

    if (PropertySlider)
    {
        PropertySlider->OnValueChanged.AddDynamic(this, &UMjPropertyRow::HandleSliderChanged);

        // Force slider to fill available horizontal space
        if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(PropertySlider->Slot))
        {
            HBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }
    }

    if (PropertyToggle)
    {
        PropertyToggle->OnCheckStateChanged.AddDynamic(this, &UMjPropertyRow::HandleToggleChanged);
    }
}

void UMjPropertyRow::InitializeProperty(const FString& InName, EMjPropertyType InType, float InitialValue, FVector2D Range, const FString& InDisplayName)
{
    PropertyName = InName;
    PropertyType = InType;

    if (PropertyLabel)
    {
        FString Display = InDisplayName.IsEmpty() ? InName : InDisplayName;
        PropertyLabel->SetText(FText::FromString(Display));
    }

    // Set UI visibility based on type
    if (PropertySlider)
    {
        PropertySlider->SetVisibility((PropertyType == EMjPropertyType::Slider) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        SetSliderRange(Range);
    }

    if (PropertyToggle)
    {
        PropertyToggle->SetVisibility(PropertyType == EMjPropertyType::Toggle ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (ValueDisplay)
    {
        ValueDisplay->SetVisibility((PropertyType == EMjPropertyType::Header) ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }

    // Aesthetic Styling
    FSlateFontInfo LabelFont = PropertyLabel ? PropertyLabel->GetFont() : FSlateFontInfo();
    LabelFont.Size = (PropertyType == EMjPropertyType::Header) ? 14 : 11;
    LabelFont.TypefaceFontName = (PropertyType == EMjPropertyType::Header) ? TEXT("Bold") : TEXT("Regular");

    FLinearColor TextColor = (PropertyType == EMjPropertyType::Header) ? FLinearColor(0.4f, 0.7f, 1.0f, 1.0f) : FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);

    if (PropertyLabel)
    {
        PropertyLabel->SetFont(LabelFont);
        PropertyLabel->SetColorAndOpacity(FSlateColor(TextColor));
    }
    
    if (ValueDisplay)
    {
        FSlateFontInfo ValFont = ValueDisplay->GetFont();
        ValFont.Size = 11;
        ValFont.TypefaceFontName = TEXT("Regular");
        ValueDisplay->SetFont(ValFont);
        ValueDisplay->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 1.0f, 0.6f, 1.0f))); // nice green for values
    }

    SetValue(InitialValue);

    if (PropertySlider)
    {
        FSliderStyle Style = PropertySlider->GetWidgetStyle();
        Style.BarThickness = 6.0f; // Make the bar thicker
        // Create an image size for the thumb since we can't easily change it without a new brush
        // but we can adjust the thumb image size if we have a default one. Just making the bar thicker often helps a lot.
        PropertySlider->SetWidgetStyle(Style);
    }
}

void UMjPropertyRow::SetSliderRange(FVector2D Range)
{
    if (PropertySlider)
    {
        // Add a small buffer or handle 0,0 range
        if (Range.X == 0.0f && Range.Y == 0.0f)
        {
            Range = FVector2D(-1.0f, 1.0f); // Default for unconstrained
        }
        PropertySlider->SetMinValue(Range.X);
        PropertySlider->SetMaxValue(Range.Y);
    }
}

void UMjPropertyRow::SetValue(float NewValue)
{
    if (PropertyType == EMjPropertyType::Slider && PropertySlider)
    {
        PropertySlider->SetValue(NewValue);
    }
    else if (PropertyType == EMjPropertyType::Toggle && PropertyToggle)
    {
        PropertyToggle->SetCheckedState(NewValue > 0.5f ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    }

    if (ValueDisplay)
    {
        if (PropertyType == EMjPropertyType::Toggle)
        {
            ValueDisplay->SetText(FText::FromString(NewValue > 0.5f ? TEXT("ON") : TEXT("OFF")));
        }
        else if (PropertyType != EMjPropertyType::Header)
        {
            ValueDisplay->SetText(FText::FromString(FString::Printf(TEXT("%8.3f"), NewValue)));
        }
    }
}

void UMjPropertyRow::HandleSliderChanged(float Value)
{
    if (PropertyType == EMjPropertyType::Slider)
    {
        if (ValueDisplay)
        {
            ValueDisplay->SetText(FText::FromString(FString::Printf(TEXT("%8.3f"), Value)));
        }
        OnValueChanged.Broadcast(Value, PropertyName);
    }
}

void UMjPropertyRow::HandleToggleChanged(bool bChecked)
{
    if (PropertyType == EMjPropertyType::Toggle)
    {
        float Val = bChecked ? 1.0f : 0.0f;
        if (ValueDisplay)
        {
            ValueDisplay->SetText(FText::FromString(bChecked ? TEXT("ON") : TEXT("OFF")));
        }
        OnValueChanged.Broadcast(Val, PropertyName);
    }
}

bool UMjPropertyRow::IsBeingDragged() const
{
    return PropertySlider && PropertySlider->HasMouseCapture();
}

void UMjPropertyRow::SetRowEnabled(bool bEnabled)
{
    if (PropertySlider)
    {
        PropertySlider->SetIsEnabled(bEnabled);
    }
    if (PropertyToggle)
    {
        PropertyToggle->SetIsEnabled(bEnabled);
    }
}
