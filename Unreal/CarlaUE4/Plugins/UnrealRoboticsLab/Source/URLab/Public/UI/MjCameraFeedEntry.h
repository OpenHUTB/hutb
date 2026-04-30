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
#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "MjCameraFeedEntry.generated.h"

class UTextBlock;
class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * @class UMjCameraFeedEntry
 * @brief UMG widget row that binds to a single UMjCamera and displays its live feed.
 *
 * Its Blueprint counterpart (WBP_MjCameraFeedEntry) must contain:
 *   - UTextBlock "CameraNameText"  — displays MjCamera::MjName
 *   - UImage     "FeedImage"       — shows the render target via a UMaterialInstanceDynamic
 *
 * Usage:
 *   UMjCameraFeedEntry* Entry = CreateWidget<UMjCameraFeedEntry>(..., CameraFeedEntryClass);
 *   Entry->BindToCamera(Cam);
 *   MyVerticalBox->AddChildToVerticalBox(Entry);
 *
 * The owning widget should call UpdateFeed() each tick and UnbindCamera() before
 * switching the articulation selection, so streaming is cleanly disabled on old cameras.
 */
UCLASS()
class URLAB_API UMjCameraFeedEntry : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * @brief Binds this entry to a camera.
     *        Enables streaming on the camera, creates a MaterialInstanceDynamic,
     *        and wires the render target into the FeedImage brush.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void BindToCamera(UMjCamera* InCamera);

    /**
     * @brief Disables streaming on the bound camera and clears internal state.
     *        Call this before the entry is removed from the panel.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void UnbindCamera();

    /**
     * @brief Called each tick by the owning widget to keep the image brush valid.
     *        Currently a no-op (MID samples the RT automatically), kept for future use.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void UpdateFeed();

    /** @brief Returns the bound camera (for streaming teardown from the owning widget). */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    UMjCamera* GetBoundCamera() const { return BoundCamera; }

protected:
    /** @brief Camera name label. Must exist in the UMG Blueprint. */
    UPROPERTY(meta = (BindWidget))
    UTextBlock* CameraNameText;

    /** @brief Live feed image. Must exist in the UMG Blueprint. */
    UPROPERTY(meta = (BindWidget))
    UImage* FeedImage;

private:
    /** @brief (Re-)applies the FSlateBrush to FeedImage using the current render target. */
    void RefreshBrush();

    /** @brief Depth mode: CPU-copy the R32f RT into DepthPreviewTexture as
     *         grayscale, normalised by the camera's DepthNear/Far range. */
    void UpdateDepthPreview();

    UPROPERTY()
    UMjCamera* BoundCamera = nullptr;

    UPROPERTY()
    UMaterialInstanceDynamic* FeedMID = nullptr;

    /** Slate-displayable BGRA texture used as the brush resource when the bound
     *  camera is in Depth mode. Null for other modes. */
    UPROPERTY(Transient)
    UTexture2D* DepthPreviewTexture = nullptr;

    /** Scratch buffer reused by the depth CPU readback to avoid per-tick allocation. */
    TArray<FLinearColor> DepthReadbackScratch;
};
