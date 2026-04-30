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

#include "UI/MjCameraFeedEntry.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"

void UMjCameraFeedEntry::BindToCamera(UMjCamera* InCamera)
{
    if (!InCamera) return;
    BoundCamera = InCamera;

    InCamera->SetStreamingEnabled(true);

    if (CameraNameText)
    {
        CameraNameText->SetText(FText::FromString(InCamera->MjName));
        FSlateFontInfo FontInfo = CameraNameText->GetFont();
        FontInfo.Size = 12;
        FontInfo.TypefaceFontName = TEXT("Bold");
        CameraNameText->SetFont(FontInfo);
        CameraNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.9f, 1.0f, 1.0f)));
    }

    RefreshBrush();
}

void UMjCameraFeedEntry::RefreshBrush()
{
    if (!BoundCamera || !FeedImage) return;

    UTextureRenderTarget2D* RT = BoundCamera->RenderTarget;
    if (!RT) return;

    const float W = 320.f;
    const float H = (BoundCamera->Resolution.X > 0)
        ? W * static_cast<float>(BoundCamera->Resolution.Y) / static_cast<float>(BoundCamera->Resolution.X)
        : W * 0.75f;

    // Depth RT is R32f — slate can't display it directly. Build (or reuse)
    // a BGRA UTexture2D preview and bind THAT as the brush. For all other
    // modes the RT is BGRA8 and we can bind it directly.
    UObject* BrushResource = nullptr;
    if (BoundCamera->CaptureMode == EMjCameraMode::Depth)
    {
        if (!DepthPreviewTexture
            || DepthPreviewTexture->GetSizeX() != BoundCamera->Resolution.X
            || DepthPreviewTexture->GetSizeY() != BoundCamera->Resolution.Y)
        {
            DepthPreviewTexture = UTexture2D::CreateTransient(
                BoundCamera->Resolution.X, BoundCamera->Resolution.Y, PF_B8G8R8A8,
                TEXT("URLabDepthPreview"));
            DepthPreviewTexture->CompressionSettings = TC_VectorDisplacementmap;
            DepthPreviewTexture->SRGB = false;
            DepthPreviewTexture->UpdateResource();
        }
        BrushResource = DepthPreviewTexture;
    }
    else
    {
        DepthPreviewTexture = nullptr;
        BrushResource = RT;
    }

    FeedImage->SetBrushResourceObject(BrushResource);

    FSlateBrush Brush = FeedImage->GetBrush();
    Brush.DrawAs    = ESlateBrushDrawType::Image;
    Brush.ImageType = ESlateBrushImageType::FullColor;
    Brush.Tiling    = ESlateBrushTileType::NoTile;
    Brush.ImageSize = FVector2D(W, H);
    FeedImage->SetBrush(Brush);

    UE_LOG(LogURLab, Log,
        TEXT("[MjCameraFeedEntry] Brush set: '%s' mode=%s RT=%dx%d display=%.0fx%.0f"),
        *BoundCamera->MjName, *UEnum::GetValueAsString(BoundCamera->CaptureMode),
        RT->SizeX, RT->SizeY, W, H);
}

void UMjCameraFeedEntry::UpdateDepthPreview()
{
    if (!BoundCamera || !DepthPreviewTexture) return;
    UTextureRenderTarget2D* RT = BoundCamera->RenderTarget;
    if (!RT) return;

    FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
    if (!Res) return;

    // Read the depth RT into FLinearColor pixels. The R channel holds linear
    // scene depth in centimetres (Unreal's world units).
    if (!Res->ReadLinearColorPixels(DepthReadbackScratch)) return;

    const int32 SizeX = RT->SizeX;
    const int32 SizeY = RT->SizeY;
    if (DepthReadbackScratch.Num() != SizeX * SizeY) return;

    const float Near = FMath::Max(0.1f, BoundCamera->DepthNearCm);
    const float Far  = FMath::Max(Near + 1.0f, BoundCamera->DepthFarCm);
    const float InvRange = 1.0f / (Far - Near);

    // Update the platform texture's mip0 pixels. BGRA byte order.
    FTexture2DMipMap& Mip = DepthPreviewTexture->GetPlatformData()->Mips[0];
    uint8* Dst = (uint8*)Mip.BulkData.Lock(LOCK_READ_WRITE);
    for (int32 i = 0; i < DepthReadbackScratch.Num(); ++i)
    {
        const float Depth = DepthReadbackScratch[i].R;
        const float Norm  = FMath::Clamp((Depth - Near) * InvRange, 0.0f, 1.0f);
        const uint8 Gray  = (uint8)FMath::RoundToInt(Norm * 255.0f);
        Dst[i * 4 + 0] = Gray;  // B
        Dst[i * 4 + 1] = Gray;  // G
        Dst[i * 4 + 2] = Gray;  // R
        Dst[i * 4 + 3] = 255;   // A
    }
    Mip.BulkData.Unlock();
    DepthPreviewTexture->UpdateResource();
}

void UMjCameraFeedEntry::UnbindCamera()
{
    if (BoundCamera)
    {
        BoundCamera->SetStreamingEnabled(false);
        BoundCamera = nullptr;
    }
    FeedMID = nullptr;
    DepthPreviewTexture = nullptr;
    DepthReadbackScratch.Reset();
    if (FeedImage) FeedImage->SetBrush(FSlateBrush());
}

void UMjCameraFeedEntry::UpdateFeed()
{
    if (!BoundCamera || !FeedImage || !BoundCamera->RenderTarget) return;

    if (!FeedImage->GetBrush().GetResourceObject())
    {
        RefreshBrush();
    }

    if (BoundCamera->CaptureMode == EMjCameraMode::Depth)
    {
        UpdateDepthPreview();
    }
}
