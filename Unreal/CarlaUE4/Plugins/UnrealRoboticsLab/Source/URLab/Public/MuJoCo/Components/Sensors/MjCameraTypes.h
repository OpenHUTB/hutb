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

#pragma once

#include "CoreMinimal.h"
#include "MjCameraTypes.generated.h"

/**
 * @enum EMjCameraMode
 * @brief What a UMjCamera captures. Read at SetStreamingEnabled(true) time —
 *        to change mode on a running camera, toggle streaming off then on.
 *
 * Lives in its own header so UMjDebugVisualizer can reference the enum without
 * pulling in the full UMjCamera include chain (SceneCaptureComponent2D, etc).
 */
UENUM(BlueprintType)
enum class EMjCameraMode : uint8
{
    Real                 UMETA(DisplayName = "Photoreal RGB"),
    Depth                UMETA(DisplayName = "Depth"),
    SemanticSegmentation UMETA(DisplayName = "Semantic Segmentation"),
    InstanceSegmentation UMETA(DisplayName = "Instance Segmentation"),
};
