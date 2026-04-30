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

#include "Utils/MeshUtils.h"

#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "Utils/URLabLogging.h"


ALandscape* MeshUtils::FindLandscapeActor(UWorld* World) {
    if (!World) {
        UE_LOG(LogURLabGenerator, Warning, TEXT("World is null!"));
        return nullptr;
    }

    for (TActorIterator<ALandscape> It(World); It; ++It) {
        ALandscape* Landscape = *It;
        if (Landscape) {
            UE_LOG(LogURLabGenerator, Log, TEXT("Found a landscape actor: %s"), *Landscape->GetName());
            return Landscape;
        }
    }

    return nullptr;
}
