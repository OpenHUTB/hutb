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
#include "UObject/StrongObjectPtr.h"

// 构造函数：UFactory 子类会被编辑器自动注册，下面四项配置决定了
// 本工厂在“导入”流程中的参与方式和产物类型。
UMujocoImportFactory::UMujocoImportFactory()
{
    // 声明本工厂能处理的文件格式，格式为 "扩展名;显示名称"。
    // 效果：.xml 文件会出现在导入对话框的格式下拉框里，显示为 "MuJoCo XML File"。
    Formats.Add(TEXT("xml;MuJoCo XML File"));

    // 指定导入产物的资产类型为蓝图（对应 FactoryCreateFile 中创建、
    // 以 AMjArticulation 为父类的 UBlueprint）。编辑器据此把本工厂用于蓝图导入。
    SupportedClass = UBlueprint::StaticClass();

    // 关闭“从零新建资产”：本工厂依赖源 .xml 文件，因此不出现在“新建资产”菜单，
    // 只能通过导入已有文件触发。
    bCreateNew = false;

    // 开启“编辑器导入”：允许把 .xml 拖进内容浏览器或走“导入”菜单，
    // 从而触发 FactoryCanImport() 和 FactoryCreateFile()。
    bEditorImport = true;
}

bool UMujocoImportFactory::FactoryCanImport(const FString& Filename)
{
    return FPaths::GetExtension(Filename).Equals(TEXT("xml"), ESearchCase::IgnoreCase);
}

// 参数详见 MujocoImportFactory.h；这里标注阅读时最关键的三个参数。
UObject* UMujocoImportFactory::FactoryCreateFile(
    UClass* InClass,
    UObject* InParent,                // 目标包：新蓝图挂在该包下；包名 = 拖放文件夹 + 资产名（决定“放哪”）
    FName InName,                     // 新资产名：由源文件基名推导，不含扩展名（决定“叫什么”）
    EObjectFlags Flags,
    const FString& Filename,          // 源 .xml 文件在磁盘上的绝对路径（决定“从哪来”）
    const TCHAR* Parms,
    FFeedbackContext* Warn,
    bool& bOutOperationCanceled)
{
    // 导入流程概览（进度条共 4 步）：
    //   步骤 0：调用 clean_meshes.py 预处理网格，生成 _ue.xml（失败则回退原始 XML）
    //   步骤 1：把原始 XML 路径写入蓝图 CDO，供运行时使用（注意：这一步并不解析 XML）
    //   步骤 2：生成 SCS 组件树（真正的解析、建节点、导入网格/材质都在 GenerateForBlueprint 内）
    //   步骤 3：编译蓝图，等待异步着色器编译并刷新渲染
    // 成功与否由返回值是否为 nullptr 表示；只有 NewBP 创建成功才会进入后续步骤。
    // 目前不对大文件做导入前的警告/确认，保持静默。

    // 创建蓝图：产物是拖入文件夹下、以 AMjArticulation 为父类的 UBlueprint
    UClass* ParentClass = AMjArticulation::StaticClass();

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
        // 进度条共 4 格，与下方 4 个 EnterProgressFrame 一一对应
        FScopedSlowTask SlowTask(4.f, NSLOCTEXT("URLab", "ImportingMuJoCo", "Importing MuJoCo model..."));
        SlowTask.MakeDialog(/*bShowCancelButton=*/false);

        // 步骤 0/4：网格预处理。
        // MuJoCo 的 STL/OBJ 直接导入 UE 效果差（没有材质、退化几何、难以合并），
        // 脚本会清洗网格并转换为 GLB/FBX，同时生成 _ue.xml；脚本不存在或失败时回退原 XML。
        SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "ImportStep0", "Preparing meshes..."));

        
        FString ActualXmlPath = Filename;   // 准备一份“实际要用的 XML 路径”，默认先等于原始 XML。
        {
            // IPluginManager::Get()                   ← 拿到“插件管理员”（全局唯一）
            // .FindPlugin("UnrealRoboticsLab")        ← 问它：叫这个名字的插件在哪？
            // ->GetBaseDir()                          ← 插件对象：我装在哪个文件夹？
            // PluginDir目的也就是找到 UnrealRoboticsLab 插件的安装目录，便于定位 Scripts/clean_meshes.py。
            FString PluginDir = IPluginManager::Get().FindPlugin("UnrealRoboticsLab")->GetBaseDir();
            FString ScriptPath = FPaths::Combine(PluginDir, TEXT("Scripts/clean_meshes.py"));

            if (FPaths::FileExists(ScriptPath))
            {
                bool bCancelled = false;
                // 在运行 clean_meshes.py 之前做"Python 环境准备"的：找到可用的 Python 解释器、
                // 确认依赖包齐全（必要时自动安装），并返回解释器路径。
                FString PythonExe = FMjPythonHelper::EnsurePythonReady(bCancelled);

                if (bCancelled)
                {
                    // Python 环境准备被用户取消：本次导入失败。
                    UE_LOG(LogURLabEditor, Log, TEXT("Import cancelled by user during Python setup."));
                    return nullptr;
                }

                if (!PythonExe.IsEmpty())
                {
                    // 运行清理脚本
                    int32 ReturnCode = -1;  // 准备接退出码
                    FString StdOut, StdErr; // 准备接输出内容
                    FString Args = FString::Printf(TEXT("\"%s\" \"%s\""), *ScriptPath, *Filename);  // 拼参数
                    UE_LOG(LogURLabEditor, Log, TEXT("Running mesh preparation: %s %s"), *PythonExe, *Args);
                    // 主要是为了把原始 XML 传给 clean_meshes.py，让它知道要处理哪个文件。
                    // PythonExe 是解释器路径，Args 是 clean_meshes.py 的路径和原始 XML 路径
                    FPlatformProcess::ExecProcess(*PythonExe, *Args, &ReturnCode, &StdOut, &StdErr);

                    if (ReturnCode == 0)
                    {
                        // 怎么从g1.xml变成g1_ue.xml的
                        // FPaths::GetPath(...):/home/ubuntu/.../unitree_g1（去掉文件名，只留目录）
                        // FPaths::GetBaseFilename(...):g1（去掉目录和扩展名，只留文件名）
                        FString UeXmlPath = FPaths::Combine(
                            FPaths::GetPath(Filename),
                            FPaths::GetBaseFilename(Filename) + TEXT("_ue.xml"));

                        if (FPaths::FileExists(UeXmlPath))
                        {
                            UE_LOG(LogURLabEditor, Log, TEXT("Using prepared XML: %s"), *UeXmlPath);
                            ActualXmlPath = UeXmlPath;  // 改用预处理生成的 _ue.xml
                        }
                    }
                    else
                    {
                        // 预处理失败不中断导入：回退到原始 XML，通常仍能导入（网格质量可能差一些）
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

        // 步骤 1/4：记录 XML 来源。
        // 进度条文案是 "Reading XML..."，但这里并不解析 XML（解析在步骤 2 的 GenerateForBlueprint 内完成）。
        SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "ImportStep1", "Reading XML..."));  // 在编辑器弹出的进度条下方显示文字（有延迟）

        // 在类默认对象（Class Default Object, CDO）中设置 XML 路径使其持久化（使用原始路径，而不是生成的 _ue 变体）
        AMjArticulation* CDO = Cast<AMjArticulation>(NewBP->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            CDO->MuJoCoXMLFile.FilePath = Filename;
            CDO->MarkPackageDirty();  // 通知编辑器：该蓝图默认对象已经改变，否则编辑器不会认为资源需要保存
        }

        // 步骤 2/4：生成 SCS 组件树。
        // 真正的 XML 解析、SCS 节点创建、网格/材质导入都在 GenerateForBlueprint 内完成。
        SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "ImportStep2", "Building Blueprint components..."));

        // 使用（可能已准备好的）XML 生成组件
        UMujocoGenerationAction* Generator = NewObject<UMujocoGenerationAction>();
        // Datasmith 网格导入结束时会显式调用 CollectGarbage()（见 MujocoMeshImporter.cpp
        // 中 TStrongObjectPtr 处的说明）。Generator 只被 C++ 栈引用，GC 看不到它；
        // 不保持在根集里的话，生成过程中它会被回收，之后继续调用它的成员函数就会崩溃。
        TStrongObjectPtr<UMujocoGenerationAction> GeneratorGuard(Generator);

        // NewBP这里的蓝图用的不是原来的xml，但是这里的ActualXmlPath用的不是新的_ue.xml？
        // 蓝图“搭组件树”用 _ue.xml，但蓝图里“记录模型来源”用的是原始 XML
        // 1._ue.xml 是“给 UE 用的派生图纸”：脚本清洗网格、转换成 GLB、解决了命名冲突，
        // 把网格路径改写成 UE 能导入的形式。用它来建组件树、导入资源，才能成功。
        // 2.原始 XML 是“给 MuJoCo 用的正版模型”：运行时仿真加载的是它；_ue.xml 只是编辑器导入过程的
        // 中间产物，不应该成为仿真源。蓝图里记录原始路径，也保证了“这个蓝图对应哪个模型文件”可追溯。
        Generator->GenerateForBlueprint(NewBP, ActualXmlPath);

        // 步骤 3/4：编译与收尾。
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
