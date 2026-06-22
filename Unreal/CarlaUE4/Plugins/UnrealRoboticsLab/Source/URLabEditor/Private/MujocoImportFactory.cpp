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


#include "MujocoImportFactory.h"
#include "MujocoGenerationAction.h"
#include "MjPythonHelper.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Interfaces/IPluginManager.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "URLabEditorLogging.h"

UMujocoImportFactory::UMujocoImportFactory()
{
    Formats.Add(TEXT("xml;MuJoCo XML File"));
    SupportedClass = UBlueprint::StaticClass();
    bCreateNew = false;
    bEditorImport = true;
}

bool UMujocoImportFactory::FactoryCanImport(const FString& Filename)
{
    return FPaths::GetExtension(Filename).Equals(TEXT("xml"), ESearchCase::IgnoreCase);
}

UObject* UMujocoImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    // Warn user if we are importing something large?
    // For now silent.

    // Create blueprint based on AMjArticulation
    UClass* ParentClass = AMjArticulation::StaticClass();
    
    // Create the Blueprint Asset
    UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
        ParentClass, 
        InParent, 
        InName, 
        BPTYPE_Normal, 
        UBlueprint::StaticClass(), 
        UBlueprintGeneratedClass::StaticClass()
    );

    if (NewBP)
    {
        FScopedSlowTask SlowTask(4.f, NSLOCTEXT("URLab", "ImportingMuJoCo", "Importing MuJoCo model..."));
        SlowTask.MakeDialog(/*bShowCancelButton=*/false);

        // Step 0: Try to run clean_meshes_trimesh.py to prepare meshes
        SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "ImportStep0", "Preparing meshes..."));

        FString ActualXmlPath = Filename;
        {
            FString PluginDir = IPluginManager::Get().FindPlugin("UnrealRoboticsLab")->GetBaseDir();
            FString ScriptPath = FPaths::Combine(PluginDir, TEXT("Scripts/clean_meshes.py"));

            if (FPaths::FileExists(ScriptPath))
            {
                bool bCancelled = false;
                FString PythonExe = FMjPythonHelper::EnsurePythonReady(bCancelled);

                if (bCancelled)
                {
                    UE_LOG(LogURLabEditor, Log, TEXT("Import cancelled by user during Python setup."));
                    return nullptr;
                }

                if (!PythonExe.IsEmpty())
                {
                    // Run the clean script
                    int32 ReturnCode = -1;
                    FString StdOut, StdErr;
                    FString Args = FString::Printf(TEXT("\"%s\" \"%s\""), *ScriptPath, *Filename);
                    UE_LOG(LogURLabEditor, Log, TEXT("Running mesh preparation: %s %s"), *PythonExe, *Args);
                    FPlatformProcess::ExecProcess(*PythonExe, *Args, &ReturnCode, &StdOut, &StdErr);

                    if (ReturnCode == 0)
                    {
                        FString UeXmlPath = FPaths::Combine(
                            FPaths::GetPath(Filename),
                            FPaths::GetBaseFilename(Filename) + TEXT("_ue.xml"));

                        if (FPaths::FileExists(UeXmlPath))
                        {
                            UE_LOG(LogURLabEditor, Log, TEXT("Using prepared XML: %s"), *UeXmlPath);
                            ActualXmlPath = UeXmlPath;
                        }
                    }
                    else
                    {
                        UE_LOG(LogURLabEditor, Warning, TEXT("Mesh preparation script failed (code %d). Using original XML."), ReturnCode);
                        if (!StdErr.IsEmpty()) UE_LOG(LogURLabEditor, Warning, TEXT("  stderr: %s"), *StdErr);
                    }
                }
                else
                {
                    UE_LOG(LogURLabEditor, Log, TEXT("Python not configured — skipping mesh preparation."));
                }
            }
        }

        SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "ImportStep1", "Reading XML..."));  // 在编辑器弹出的进度条下方显示文字（有延迟）

        // 在类默认对象（Class Default Object, CDO）中设置 XML 路径使其持久化（使用原始路径，而不是生成的 _ue 变体）
        AMjArticulation* CDO = Cast<AMjArticulation>(NewBP->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->MuJoCoXMLFile.FilePath = Filename;
            CDO->MarkPackageDirty();  // 通知编辑器：该蓝图默认对象已经改变，否则编辑器不会认为资源需要保存
        }

        SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "ImportStep2", "Building Blueprint components..."));

        // 使用（可能已准备好的）XML 生成组件
        UMujocoGenerationAction* Generator = NewObject<UMujocoGenerationAction>();
        Generator->GenerateForBlueprint(NewBP, ActualXmlPath);

        SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "ImportStep3", "Compiling Blueprint..."));

        // 编译以保存更改并确保组件有效
        FKismetEditorUtilities::CompileBlueprint(NewBP);

        // 等待所有着色器编译完成并刷新渲染命令。
        // 导入过程中创建的材质实例会触发异步着色器编译。
        // 如果内容浏览器在着色器准备就绪之前渲染缩略图，
        // 则渲染线程会崩溃 (UE-23902)。
        if (GShaderCompilingManager)
        {
            GShaderCompilingManager->FinishAllCompilation();
        }
        FlushRenderingCommands();
    }
    
    bOutOperationCanceled = false;
    return NewBP;
}
