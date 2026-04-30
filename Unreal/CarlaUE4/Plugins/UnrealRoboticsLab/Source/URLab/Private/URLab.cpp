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

#include "URLab.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "Utils/URLabLogging.h"
#include "Interfaces/IPluginManager.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#endif

#define LOCTEXT_NAMESPACE "FURLabModule"
void FURLabModule::StartupModule()
{
    FString PluginDir = IPluginManager::Get().FindPlugin("UnrealRoboticsLab")->GetBaseDir();
    FString InstallDir = FPaths::Combine(PluginDir, TEXT("third_party/install"));

    // Function to load a DLL and log success/failure
    auto LoadDependencyDLL = [&](const FString& LibraryName, const FString& SubDir) {
        // Try plugin third-party path first (editor / development)
        FString DLLPath = FPaths::Combine(InstallDir, SubDir, TEXT("bin"), LibraryName);
        if (!FPaths::FileExists(DLLPath))
        {
            // Packaged build: DLLs staged next to the executable
            DLLPath = FPaths::Combine(FPlatformProcess::GetModulesDirectory(), LibraryName);
        }
        if (FPaths::FileExists(DLLPath)) {
            void* Handle = FPlatformProcess::GetDllHandle(*DLLPath);
            if (Handle) {
                UE_LOG(LogURLab, Log, TEXT("Loaded %s from %s"), *LibraryName, *DLLPath);
                return true;
            } else {
                UE_LOG(LogURLab, Error, TEXT("Failed to load %s from %s"), *LibraryName, *DLLPath);
            }
        } else {
            UE_LOG(LogURLab, Error, TEXT("%s not found (searched plugin dir + binary dir)"), *LibraryName);
        }
        return false;
    };

    // Load MuJoCo. Since 3.7.0 the obj/stl decoders are compiled into
    // mujoco.dll itself (changelog item 9); loading the standalone
    // obj_decoder.dll / stl_decoder.dll would cause a plugin-registration
    // collision and crash during module init.
    LoadDependencyDLL(TEXT("mujoco.dll"), TEXT("MuJoCo"));

    // Load ZMQ
    LoadDependencyDLL(TEXT("libzmq-v143-mt-4_3_6.dll"), TEXT("libzmq"));

    // Load CoACD (Shared library)
    LoadDependencyDLL(TEXT("lib_coacd.dll"), TEXT("CoACD"));

    // Some CoACD dependencies like TBB or runtimes might be in CoACD/bin
    // They should be loaded automatically if in search path, but we can verify here if needed.

#if WITH_EDITOR
    // Register custom asset category
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    MuJoCoAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("MuJoCo")), LOCTEXT("MujocoAssetCategory", "MuJoCo"));
    UE_LOG(LogURLab, Log, TEXT("Registered MuJoCo Asset Category with bitmask: %u"), MuJoCoAssetCategoryBit);
#endif
}

void FURLabModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FURLabModule, URLab)
