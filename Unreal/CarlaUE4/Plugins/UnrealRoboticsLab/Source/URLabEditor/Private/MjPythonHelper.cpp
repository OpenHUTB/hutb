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

#include "MjPythonHelper.h"
#include "URLabEditorLogging.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"

FString FMjPythonHelper::GetLocalIniPath()
{
    TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin("UnrealRoboticsLab");
    if (!Plugin.IsValid())
    {
        UE_LOG(LogURLabEditor, Warning, TEXT("[Python] Could not find UnrealRoboticsLab plugin. Using project config dir."));
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir() / TEXT("UnrealRoboticsLab") / TEXT("Config") / TEXT("LocalUnrealRoboticsLab.ini"));
    }
    FString FullPath = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Config") / TEXT("LocalUnrealRoboticsLab.ini"));
    UE_LOG(LogURLabEditor, Verbose, TEXT("[Python] Local INI path: %s"), *FullPath);
    return FullPath;
}

FString FMjPythonHelper::GetUEBundledPythonPath()
{
    FString PythonDir = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Python3");
#if PLATFORM_WINDOWS
    FString Path = PythonDir / TEXT("Win64/python.exe");
#elif PLATFORM_LINUX
    FString Path = PythonDir / TEXT("Linux/bin/python3");
#elif PLATFORM_MAC
    FString Path = PythonDir / TEXT("Mac/bin/python3");
#else
    FString Path;
#endif
    return FPaths::ConvertRelativePathToFull(Path);
}

FString FMjPythonHelper::GetStoredPythonOverride()
{
    FString IniPath = GetLocalIniPath();
    if (!FPaths::FileExists(IniPath)) return FString();

    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *IniPath)) return FString();

    // 解析简单的 INI 文件：查找 PythonPath= 行
    TArray<FString> Lines;
    FileContent.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        FString Trimmed = Line.TrimStartAndEnd();
        if (Trimmed.StartsWith(TEXT("PythonPath=")))
        {
            return Trimmed.Mid(11).TrimStartAndEnd();
        }
    }
    return FString();
}

void FMjPythonHelper::StorePythonOverride(const FString& PythonPath)
{
    FString IniPath = GetLocalIniPath();
    FString AbsPythonPath = FPaths::ConvertRelativePathToFull(PythonPath);

    // Write directly to file — GConfig can silently fail for paths outside the project
    FString IniContent = FString::Printf(TEXT("[PythonSettings]\nPythonPath=%s\n"), *AbsPythonPath);
    if (FFileHelper::SaveStringToFile(IniContent, *IniPath))
    {
        UE_LOG(LogURLabEditor, Log, TEXT("[Python] Stored Python path '%s' to config: %s"), *AbsPythonPath, *IniPath);
    }
    else
    {
        UE_LOG(LogURLabEditor, Warning, TEXT("[Python] Failed to write config file: %s"), *IniPath);
    }
}

FString FMjPythonHelper::ResolvePythonPath()
{
    // 1. 检查用户覆盖设置
    FString Override = GetStoredPythonOverride();
    if (!Override.IsEmpty() && FPaths::FileExists(Override))
    {
        return Override;
    }

    // 2. 回退到 UE 内置的 Python
    FString Bundled = GetUEBundledPythonPath();
    if (FPaths::FileExists(Bundled))
    {
        return Bundled;
    }

    // 3. 尝试使用 PATH 系统作为最后手段
    return TEXT("python");
}

bool FMjPythonHelper::ValidatePythonBinary(const FString& PythonPath)
{
    int32 ReturnCode = -1;
    FString StdOut, StdErr;
    FPlatformProcess::ExecProcess(*PythonPath, TEXT("--version"), &ReturnCode, &StdOut, &StdErr);
    if (ReturnCode == 0)
    {
        FString Version = StdOut.IsEmpty() ? StdErr : StdOut; // Python 2 prints to stderr
        UE_LOG(LogURLabEditor, Log, TEXT("[Python] Validated: %s -> %s"), *PythonPath, *Version.TrimStartAndEnd());
        return true;
    }
    return false;
}

bool FMjPythonHelper::CheckPythonPackages(const FString& PythonPath)
{
    int32 ReturnCode = -1;
    FString StdOut, StdErr;
    FPlatformProcess::ExecProcess(*PythonPath, TEXT("-c \"import trimesh; import numpy; import scipy\""), &ReturnCode, &StdOut, &StdErr);
    return (ReturnCode == 0);
}

bool FMjPythonHelper::InstallPythonPackages(const FString& PythonPath, FString& OutLog)
{
    // First check if pip is available
    {
        int32 PipCheck = -1;
        FString PipOut, PipErr;
        FPlatformProcess::ExecProcess(*PythonPath, TEXT("-m pip --version"), &PipCheck, &PipOut, &PipErr);
        if (PipCheck != 0)
        {
            OutLog = FString::Printf(
                TEXT("pip is not available in this Python environment.\n\n")
                TEXT("Please install pip first, or choose a different Python interpreter.\n")
                TEXT("You can also install packages manually:\n")
                TEXT("  %s -m ensurepip\n")
                TEXT("  %s -m pip install trimesh numpy scipy"),
                *PythonPath, *PythonPath);
            UE_LOG(LogURLabEditor, Warning, TEXT("[Python] pip not available: %s"), *PipErr);
            return false;
        }
    }

    int32 ReturnCode = -1;
    FString StdOut, StdErr;
    UE_LOG(LogURLabEditor, Log, TEXT("[Python] Installing packages: %s -m pip install trimesh numpy scipy"), *PythonPath);
    FPlatformProcess::ExecProcess(*PythonPath, TEXT("-m pip install trimesh numpy scipy"), &ReturnCode, &StdOut, &StdErr);
    OutLog = StdOut + TEXT("\n") + StdErr;
    if (ReturnCode == 0)
    {
        UE_LOG(LogURLabEditor, Log, TEXT("[Python] Package install succeeded."));
        return true;
    }
    UE_LOG(LogURLabEditor, Warning, TEXT("[Python] Package install failed (code %d): %s"), ReturnCode, *OutLog);
    return false;
}

FString FMjPythonHelper::EnsurePythonReady(bool& bOutCancelled)
{
    bOutCancelled = false;
    // 这台机器上还没有保存过 Python 解释器路径配置
    bool bNeedsFirstTimeSetup = GetStoredPythonOverride().IsEmpty();

    FString PythonPath = ResolvePythonPath();
    bool bIsUEBundled = (PythonPath == GetUEBundledPythonPath());

    // 验证已解析的 Python
    if (!ValidatePythonBinary(PythonPath))
    {
        FText Title = FText::FromString(TEXT("未找到 Python 解释器"));
        FText Message = FText::FromString(FString::Printf(
            TEXT("URLab 在 MJCF 导入过程中使用 Python 对网格文件进行预处理。\n\n")
            TEXT("这是因为 Unreal Engine 原生不支持 MuJoCo 使用的所有网格格式")
            TEXT("（例如 STL、某些带特殊特性的 OBJ）。预处理步骤会把网格转换为")
            TEXT("兼容格式，并解决资产命名冲突。\n\n")
            TEXT("在以下路径未找到有效的 Python 解释器：\n%s\n\n")
            TEXT("点击“是”浏览选择 Python 解释器，或点击“取消”取消本次导入。"),
            *PythonPath));

        EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNoCancel, Message, &Title);
        if (Result == EAppReturnType::Cancel)
        {
            bOutCancelled = true;
            return FString();
        }
        if (Result == EAppReturnType::No)
        {
            return FString(); // 跳过预处理
        }

        // 浏览 Python
        PythonPath = BrowseForPython();
        if (PythonPath.IsEmpty())
        {
            bOutCancelled = true;
            return FString();
        }
        bIsUEBundled = false;
    }

    // 如果缺少软件包或首次设置，显示软件包对话框
    if (!CheckPythonPackages(PythonPath) || bNeedsFirstTimeSetup)
    {
        FString EnvLabel = bIsUEBundled
            ? TEXT("Unreal Engine 自带的 Python 环境")
            : FString::Printf(TEXT("你选择的 Python：\n%s"), *PythonPath);

        bool bPackagesPresent = CheckPythonPackages(PythonPath);

        FText Title = FText::FromString(bPackagesPresent
            ? TEXT("Python 环境设置")
            : TEXT("需要安装 Python 软件包"));

        // Button mapping: Yes = proceed, No = browse for different Python, Cancel = abort import
        FString MessageStr;
        if (bPackagesPresent)
        {
            MessageStr = FString::Printf(
                TEXT("URLab 在 MJCF 导入过程中使用 Python 对网格文件进行预处理。\n\n")
                TEXT("所需软件包已在 %s 中安装。\n\n")
                TEXT("点击“是”使用此 Python，点击“否”选择其他解释器，")
                TEXT("或点击“取消”取消导入。"),
                *EnvLabel);
        }
        else
        {
            MessageStr = FString::Printf(
                TEXT("URLab 需要 'trimesh'、'numpy'、'scipy' 这些 Python 软件包来预处理网格文件。\n\n")
                TEXT("Unreal Engine 原生不支持 MuJoCo 使用的所有网格格式，")
                TEXT("因此需要这些软件包来转换并准备要导入的网格。\n\n")
                TEXT("这些软件包将安装到：%s\n\n")
                TEXT("现在安装吗？\n\n")
                TEXT("注意：安装期间编辑器会暂时无响应，")
                TEXT("可能需要几分钟左右。\n\n")
                TEXT("你也可以在自己的 Python 环境中手动安装：\n")
                TEXT("  <your-python> -m pip install trimesh numpy scipy\n")
                TEXT("然后在插件目录的 Config/LocalUnrealRoboticsLab.ini 中设置解释器路径。\n\n")
                TEXT("点击“是”开始安装，点击“否”选择其他解释器，")
                TEXT("或点击“取消”取消导入。"),
                *EnvLabel);
        }

        EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNoCancel,
           FText::FromString(MessageStr), &Title);

        if (Result == EAppReturnType::Cancel)
        {
            bOutCancelled = true;
            return FString();
        }
        else if (Result == EAppReturnType::No)
        {
            PythonPath = BrowseForPython();
            if (PythonPath.IsEmpty())
            {
                bOutCancelled = true;
                return FString();
            }

            // Re-check packages with new Python
            if (CheckPythonPackages(PythonPath))
            {
                UE_LOG(LogURLabEditor, Log, TEXT("[Python] Packages already available in selected Python."));
                StorePythonOverride(PythonPath);
                return PythonPath;
            }

            bPackagesPresent = false;

            // Ask to install for the new Python
            FText InstallMsg = FText::FromString(FString::Printf(
                TEXT("将 'trimesh'、'numpy'、'scipy' 安装到：\n%s？\n\n")
                TEXT("安装期间编辑器会暂时无响应。\n\n")
                TEXT("点击“取消”将取消本次导入。"), *PythonPath));
            if (FMessageDialog::Open(EAppMsgType::OkCancel, InstallMsg, &Title) == EAppReturnType::Cancel)
            {
                bOutCancelled = true;
                return FString();
            }
        }

        // 如果尚未安装，请安装相关软件包
        if (!bPackagesPresent)
        {
            FString InstallLog;
            const FText FailureTitle = FText::FromString(TEXT("软件包安装失败"));
            if (!InstallPythonPackages(PythonPath, InstallLog))
            {
                FMessageDialog::Open(EAppMsgType::Ok,
                    FText::FromString(FString::Printf(
                        TEXT("软件包安装失败。你可以手动运行以下命令安装：\n\n")
                        TEXT("%s -m pip install trimesh numpy scipy\n\n")
                        TEXT("错误日志：\n%s"),
                        *PythonPath, *InstallLog)),
                    &FailureTitle);
                return FString();
            }

            if (!CheckPythonPackages(PythonPath))
            {
                const FText VerifyTitle = FText::FromString(TEXT("软件包验证失败"));
                FMessageDialog::Open(EAppMsgType::Ok,
                    FText::FromString(TEXT("软件包已安装，但仍然无法导入。请检查你的 Python 环境。")),
                    &VerifyTitle);
                return FString();
            }
        }
    }

    // 把当前选定的 Python 解释器绝对路径保存成本机配置，供下次导入直接复用。
    StorePythonOverride(PythonPath);
    UE_LOG(LogURLabEditor, Log, TEXT("[Python] Ready: %s"), *PythonPath);
    return PythonPath;
}

FString FMjPythonHelper::BrowseForPython()
{
    TArray<FString> OutFiles;
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        DesktopPlatform->OpenFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            TEXT("选择 Python 解释器"),
            TEXT(""),
            TEXT(""),
#if PLATFORM_WINDOWS
            TEXT("Python 可执行文件 (*.exe)|*.exe"),
#else
            TEXT("所有文件 (*)|*"),
#endif
            0, OutFiles);
    }

    if (OutFiles.Num() == 0) return FString();

    FString PythonPath = OutFiles[0];
    if (!ValidatePythonBinary(PythonPath))
    {
        const FText Title = FText::FromString(TEXT("无效的 Python"));
        FMessageDialog::Open(EAppMsgType::Ok,
            FText::FromString(FString::Printf(TEXT("“%s”不是有效的 Python 解释器。"), *PythonPath)),
            &Title);
        return FString();
    }

    StorePythonOverride(PythonPath);
    return PythonPath;
}
