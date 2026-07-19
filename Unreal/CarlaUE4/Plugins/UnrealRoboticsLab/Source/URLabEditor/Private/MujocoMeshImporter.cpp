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

// MujocoMeshImporter.cpp — Mesh and material import methods for UMujocoGenerationAction.

#include "MujocoGenerationAction.h"
#include "URLabEditorLogging.h"
#include "Engine/StaticMesh.h"
#include "AssetToolsModule.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "AssetImportTask.h"
#include "FileHelpers.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "ImageUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

UStaticMesh* UMujocoGenerationAction::ImportSingleMesh(const FString& SourcePath, const FString& DestinationPath)
{
    if (SourcePath.IsEmpty() || DestinationPath.IsEmpty()) {
        return nullptr;
    }

    FString FileName = FPaths::GetBaseFilename(SourcePath);
    FString PackageName = FPaths::Combine(DestinationPath, FileName);
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    UStaticMesh* ExistingMesh = LoadObject<UStaticMesh>(nullptr, *PackageName);
    if (ExistingMesh) return ExistingMesh;

    UE_LOG(LogURLabEditor, Log, TEXT("Importing mesh from: %s to %s"), *SourcePath, *DestinationPath);

    // 优先考虑文件格式： FBX > GLB > GLTF > Original (OBJ/STL)
    FString ActualSourcePath = SourcePath;
    FString BasePath = FPaths::ChangeExtension(SourcePath, "");

    // 按优先顺序检查格式
    TArray<FString> Extensions = { TEXT("fbx"), TEXT("glb"), TEXT("gltf") };
    bool bFoundHigherPriority = false;

    for (const FString& Ext : Extensions)
    {
        FString PotentialPath = BasePath + TEXT(".") + Ext;
        if (FPaths::FileExists(PotentialPath))
        {
            ActualSourcePath = PotentialPath;
            bFoundHigherPriority = true;
            UE_LOG(LogURLabEditor, Log, TEXT("Found higher priority mesh file: %s"), *ActualSourcePath);
            break;
        }
    }

    // 如果没有找到高优先级格式，确保原始存在
    if (!bFoundHigherPriority && !FPaths::FileExists(ActualSourcePath))
    {
         UE_LOG(LogURLabEditor, Error, TEXT("Source mesh file does not exist: %s"), *ActualSourcePath);
         return nullptr;
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // 先试着用 MikkTSpace 导入（效果最好）
    // 使用 ActualSourcePath 替代 SourcePath
    UStaticMesh* ImportedMesh = AttemptMeshImport(ActualSourcePath, DestinationPath, EFBXNormalGenerationMethod::MikkTSpace);

    // 验证网格
    if (ImportedMesh && ValidateMesh(ImportedMesh, FileName))
    {
        UE_LOG(LogURLabEditor, Log, TEXT("Successfully imported mesh '%s' with MikkTSpace"), *FileName);
        return ImportedMesh;
    }

    // MikkTSpace 失败或网格无效 - 尝试使用内置法线作为备用
    if (ImportedMesh)
    {
        UE_LOG(LogURLabEditor, Warning, TEXT("Mesh '%s' has issues with MikkTSpace, attempting fallback with BuiltIn normals"), *FileName);
    }
    else
    {
        UE_LOG(LogURLabEditor, Warning, TEXT("Failed to import mesh '%s' with MikkTSpace, attempting fallback"), *FileName);
    }

    ImportedMesh = AttemptMeshImport(ActualSourcePath, DestinationPath, EFBXNormalGenerationMethod::BuiltIn);

    if (ImportedMesh && ValidateMesh(ImportedMesh, FileName))
    {
        UE_LOG(LogURLabEditor, Warning, TEXT("Successfully imported mesh '%s' with BuiltIn normals (fallback)"), *FileName);
        return ImportedMesh;
    }

    UE_LOG(LogURLabEditor, Error, TEXT("Failed to import mesh '%s' - all import methods failed"), *FileName);
    return nullptr;
}

UStaticMesh* UMujocoGenerationAction::AttemptMeshImport(const FString& SourcePath, const FString& DestinationPath, EFBXNormalGenerationMethod::Type NormalMethod)
{
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    // 配置自动导入任务
    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
	// 规范斜杠并转为绝对路径
	FString NormalizedSourcePath = SourcePath;
	FPaths::MakeStandardFilename(NormalizedSourcePath);                             // 使用标准分隔符（'/'）
	NormalizedSourcePath = FPaths::ConvertRelativePathToFull(NormalizedSourcePath); // 变为绝对路径
	FPaths::NormalizeFilename(NormalizedSourcePath);                                // 清理重复分隔符、末尾斜杠等
	ImportTask->Filename = NormalizedSourcePath;
    ImportTask->DestinationPath = DestinationPath;  // 导入的资产生成在：/Game/MuJoCoImports/g1_29dof_rev_1_0_ue_Assets/Meshes
    ImportTask->bAutomated = true;
    ImportTask->bSave = true;
    ImportTask->bReplaceExisting = true;
    ImportTask->bReplaceExistingSettings = true;

    // 仅配置 FBX 工厂以支持 FBX/OBJ
    FString Extension = FPaths::GetExtension(SourcePath).ToLower();

    if (Extension == "fbx" || Extension == "obj" || Extension == "t3d")
    {
        // 配置 FBX 工厂
        UFbxFactory* FbxFactory = NewObject<UFbxFactory>();
        ImportTask->Factory = FbxFactory;

        // 配置 FBX 导入 UI 设置
        UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
        ImportUI->bImportMesh = true;
        ImportUI->bImportTextures = false;
        ImportUI->bImportMaterials = false;
        ImportUI->bAutomatedImportShouldDetectType = false;
        ImportUI->MeshTypeToImport = FBXIT_StaticMesh;

        // 鲁棒的的静态网格设置
        ImportUI->StaticMeshImportData->bCombineMeshes = true;
        ImportUI->StaticMeshImportData->bRemoveDegenerates = true;
        ImportUI->StaticMeshImportData->bComputeWeightedNormals = true;
        ImportUI->StaticMeshImportData->bGenerateLightmapUVs = true;
        ImportUI->StaticMeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ComputeNormals;
        ImportUI->StaticMeshImportData->NormalGenerationMethod = NormalMethod;

        // 用于修复退化几何体（特别是来自 OBJ 文件）的其他设置
        ImportUI->StaticMeshImportData->bAutoGenerateCollision = false; // 我们单独处理碰撞。
        ImportUI->StaticMeshImportData->bBuildReversedIndexBuffer = true;
        // ImportUI->StaticMeshImportData->bBuildNanite = false; // Nanite 需要清晰的几何结构（UE4中没有Nanite）

        // 顶点焊接——对于修复导致切线退化的重叠顶点至关重要。
        // 注意：UE 5.7 中没有直接的 bWeldVertices 函数，但 bRemoveDegenerates 函数可以处理这个问题。

        // 将 UI 应用于工厂
        FbxFactory->ImportUI = ImportUI;
        FbxFactory->EnableShowOption();
    }
    else
    {
        // 对于其他格式（GLTF、GLB 等），让虚幻引擎的资源工具自动查找合适的工厂。
        // 我们无需手动设置工厂，因此 ImportAssetTasks 会自动检测正确的工厂。
        // 注意：我们会丢失一些细粒度的设置（例如 NormalGenerationMethod），
        // 但 GLTF 导入器通常依赖于文件本身的数据，这些数据通常比 OBJ 格式更清晰。
        // ImportTask->Factory = nullptr;

        UE_LOG(LogURLabEditor, Log, TEXT("Using automated factory detection for mesh: %s"), *SourcePath);
    }

    // 运行导入
    TArray<UAssetImportTask*> ImportTasks;
    ImportTasks.Add(ImportTask);
    AssetTools.ImportAssetTasks(ImportTasks);

    // 获取结果
    TArray<UObject*> ImportedAssets;
	// TArray t = ImportTask->ImportedObjectPaths;
    for (UObject* Obj : ImportTask->Result)  // for (UObject* Obj : ImportTask->GetObjects())
    {
        if (Obj) ImportedAssets.Add(Obj);
    }

    // 记录所有导入的资产以进行调试
    UE_LOG(LogURLabEditor, Log, TEXT("[ImportSingleMesh] Import returned %d objects:"), ImportedAssets.Num());
    for (int32 i = 0; i < ImportedAssets.Num(); ++i)
    {
        UObject* Obj = ImportedAssets[i];
        UE_LOG(LogURLabEditor, Log, TEXT("  [%d] %s (%s) at %s"),
            i, *Obj->GetName(), *Obj->GetClass()->GetName(), *Obj->GetPathName());
    }

    // 在所有导入的资源中搜索 StaticMesh（GLB 导入可能首先返回纹理）
    UStaticMesh* Mesh = nullptr;
    for (UObject* Obj : ImportedAssets)
    {
        Mesh = Cast<UStaticMesh>(Obj);
        if (Mesh) break;
    }

    // 如果在直接搜索结果中找不到，请搜索 Interchange 可能使用的子文件夹路径。
    if (!Mesh)
    {
        FString MeshName = FPaths::GetBaseFilename(SourcePath);

        // 尝试不同的子文件夹模式，互换使用
        TArray<FString> SearchPaths = {
            FString::Printf(TEXT("%s/%s/StaticMeshes/%s.%s"), *DestinationPath, *MeshName, *MeshName, *MeshName),
            FString::Printf(TEXT("%s/%s/StaticMeshes/%s"), *DestinationPath, *MeshName, *MeshName),
            FString::Printf(TEXT("%s/%s.%s"), *DestinationPath, *MeshName, *MeshName),
        };

        for (const FString& SearchPath : SearchPaths)
        {
            Mesh = LoadObject<UStaticMesh>(nullptr, *SearchPath);
            if (Mesh)
            {
                UE_LOG(LogURLabEditor, Log, TEXT("[ImportSingleMesh] Found mesh at: %s"), *SearchPath);
                break;
            }
            else
            {
                UE_LOG(LogURLabEditor, Log, TEXT("[ImportSingleMesh] Not found at: %s"), *SearchPath);
            }
        }

        // 最后手段：使用资源注册表查找目标文件夹中的任何 StaticMesh 文件
        if (!Mesh)
        {
            FString SearchDir = FString::Printf(TEXT("%s/%s"), *DestinationPath, *MeshName);
            UE_LOG(LogURLabEditor, Log, TEXT("[ImportSingleMesh] Searching asset registry under: %s"), *SearchDir);

            IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
            TArray<FAssetData> Assets;
            AssetRegistry.GetAssetsByPath(FName(*SearchDir), Assets, true);

            for (const FAssetData& Asset : Assets)
            {
                // UE_LOG(LogURLabEditor, Log, TEXT("  Registry: %s (%s)"), *Asset.AssetName.ToString(), *Asset.AssetClassPath.ToString());
                // if (Asset.AssetClassPath.GetAssetName() == TEXT("StaticMesh"))
                // {
                //     Mesh = Cast<UStaticMesh>(Asset.GetAsset());
                //     if (Mesh)
                //     {
                //         UE_LOG(LogURLabEditor, Log, TEXT("[ImportSingleMesh] Found mesh via registry: %s"), *Asset.GetObjectPathString());
                //         break;
                //     }
                // }
            }
        }
    }

    if (Mesh)
    {
        // 清除可能引用已剥离纹理的 Interchange 创建的材质。
        // 我们的导入流程会在 SCS 模板上分配 MI_ 材质实例，
        // 但静态网格资源会在其槽位中保留 Interchange 材质。
        // 这可能会导致在浏览/生成缩略图时渲染线程崩溃 (UE-23902)。
        // for (FStaticMaterial& Mat : Mesh->GetStaticMaterials())
        // {
        //     Mat.MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
        // }

        // 强制重建边界 - 对于修复 0x0x0 大小问题至关重要
        Mesh->Build();
        Mesh->CalculateExtendedBounds();

        UPackage* Package = Mesh->GetOutermost();
        FEditorFileUtils::PromptForCheckoutAndSave({Package}, false, false);

        UE_LOG(LogURLabEditor, Log, TEXT("Imported mesh '%s' - Bounds: %s"),
            *FPaths::GetBaseFilename(SourcePath),
            *Mesh->GetBoundingBox().GetSize().ToString());

        return Mesh;
    }

    return nullptr;
}

bool UMujocoGenerationAction::ValidateMesh(UStaticMesh* Mesh, const FString& MeshName)
{
    if (!Mesh)
    {
        UE_LOG(LogURLabEditor, Error, TEXT("Mesh validation failed: Mesh is null"));
        return false;
    }

    // 检查网格是否有渲染数据
    // if (!Mesh->GetRenderData())
    // {
    //     UE_LOG(LogURLabEditor, Error, TEXT("Mesh '%s' has no render data"), *MeshName);
    //     return false;
    // }

    // 检查 LOD 0 是否存在
    // if (Mesh->GetRenderData()->LODResources.Num() == 0)
    // {
    //     UE_LOG(LogURLabEditor, Error, TEXT("Mesh '%s' has no LOD resources"), *MeshName);
    //     return false;
    // }

    // const FStaticMeshLODResources& LOD0 = Mesh->GetRenderData()->LODResources[0];
// 
    // // 检查顶点缓冲
    // if (LOD0.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
    // {
    //     UE_LOG(LogURLabEditor, Error, TEXT("Mesh '%s' has empty vertex buffer"), *MeshName);
    //     return false;
    // }
// 
    // // 检查索引缓冲区
    // if (LOD0.IndexBuffer.GetNumIndices() == 0)
    // {
    //     UE_LOG(LogURLabEditor, Error, TEXT("Mesh '%s' has empty index buffer"), *MeshName);
    //     return false;
    // }

    // 记录网格统计信息
    int32 NumVertices; // = LOD0.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
    int32 NumTriangles; // = LOD0.IndexBuffer.GetNumIndices() / 3;
    int32 NumUVChannels; // = LOD0.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();

    UE_LOG(LogURLabEditor, Log, TEXT("Mesh '%s' validation: %d vertices, %d triangles, %d UV channels"),
        *MeshName, NumVertices, NumTriangles, NumUVChannels);

    // 如果没有 UV 通道 0，就发出警告
    if (NumUVChannels == 0)
    {
        UE_LOG(LogURLabEditor, Warning, TEXT("Mesh '%s' has no UV channels - materials may not display correctly"), *MeshName);
    }

    return true;
}


UTexture2D* UMujocoGenerationAction::ImportSingleTexture(const FString& SourcePath, const FString& DestinationPath)
{
    if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
    {
        return nullptr;
    }

    FString FileName = FPaths::GetBaseFilename(SourcePath);
    FString PackageName = FPaths::Combine(DestinationPath, FileName);
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    // Check if texture already exists
    UTexture2D* ExistingTexture = LoadObject<UTexture2D>(nullptr, *PackageName);
    if (ExistingTexture)
    {
        return ExistingTexture;
    }

    UE_LOG(LogURLabEditor, Log, TEXT("Importing texture from: %s to %s"), *SourcePath, *DestinationPath);

    // Load texture file from disk
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *SourcePath))
    {
        UE_LOG(LogURLabEditor, Error, TEXT("Failed to load texture file: %s"), *SourcePath);
        return nullptr;
    }

    // Determine image format from extension
    FString Extension = FPaths::GetExtension(SourcePath).ToLower();
    EImageFormat ImageFormat = EImageFormat::Invalid;

    if (Extension == TEXT("png"))
    {
        ImageFormat = EImageFormat::PNG;
    }
    else if (Extension == TEXT("jpg") || Extension == TEXT("jpeg"))
    {
        ImageFormat = EImageFormat::JPEG;
    }
    else if (Extension == TEXT("tga"))
    {
        // ImageFormat = EImageFormat::TGA;
    }
    else if (Extension == TEXT("bmp"))
    {
        ImageFormat = EImageFormat::BMP;
    }
    else
    {
        UE_LOG(LogURLabEditor, Warning, TEXT("Unsupported texture format: %s"), *Extension);
        return nullptr;
    }

    // Decode image
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogURLabEditor, Error, TEXT("Failed to decode texture: %s"), *SourcePath);
        return nullptr;
    }

    // Get raw image data
    TArray<uint8> RawData;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
    {
        UE_LOG(LogURLabEditor, Error, TEXT("Failed to get raw texture data: %s"), *SourcePath);
        return nullptr;
    }

    // Create package
    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();

    // Create texture
    UTexture2D* NewTexture = NewObject<UTexture2D>(Package, FName(*FileName), RF_Public | RF_Standalone);

    // Set texture properties
    NewTexture->Source.Init(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), 1, 1, TSF_BGRA8, RawData.GetData());
    NewTexture->SRGB = true;
    NewTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
    NewTexture->MipGenSettings = TextureMipGenSettings::TMGS_FromTextureGroup;
    NewTexture->UpdateResource();

    // Save package
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewTexture);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    // UPackage::SavePackage(Package, NewTexture, *PackageFileName, SaveArgs);

    UE_LOG(LogURLabEditor, Log, TEXT("Successfully imported texture: %s"), *FileName);
    return NewTexture;
}


// 基于项目内的母材质（/UnrealRoboticsLab/Materials/M_MuJoCo_Master）为导入的网格创建或复用一个 UMaterialInstanceConstant，
// 并把从 MuJoCo 材质数据解析出的颜色/纹理参数写入该实例，返回已创建或复用的实例指针。
UMaterialInstanceConstant* UMujocoGenerationAction::CreateMaterialInstance(
    const FString& MeshName,
    const FMuJoCoMaterialData& MaterialData,
    const TMap<FString, UTexture2D*>& TextureAssets,
    const FString& DestinationPath)
{
    // 加载 MuJoCo 相关资产的通用母材质模板：集中定义外观参数，包括基础色、金属度、粗糙度、法线等。
    // 应该位于 hutb\Unreal\CarlaUE4\Plugins\UnrealRoboticsLab\Content\Materials，而不是 hutb\Unreal\CarlaUE4\Content\Matrials
    // 直接复制到 hutb\Unreal\CarlaUE4\Content\Matrials 会出现资产版本不符的问题：
    // LogAssetRegistry: Error: Package ../../../../../Unreal/CarlaUE4/Plugins/UnrealRoboticsLab/Content/Materials/M_MuJoCo_Master.uasset is too old
    UMaterial* MasterMaterial = LoadObject<UMaterial>(nullptr, TEXT("/UnrealRoboticsLab/Materials/M_MuJoCo_Master.M_MuJoCo_Master"));
    if (!MasterMaterial)
    {
        UE_LOG(LogURLabEditor, Error, TEXT("Failed to load master material: /UnrealRoboticsLab/Materials/M_MuJoCo_Master"));
        return nullptr;
    }

    // 创建材质实例包
    FString InstanceName = FString::Printf(TEXT("MI_%s"), *MeshName);
    FString PackageName = FPaths::Combine(DestinationPath, InstanceName);
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    // 检查材质实例是否已存在——在同一导入会话期间重复使用（多个几何体引用同一材质），
    // 但不要跳过参数设置。
    UMaterialInstanceConstant* ExistingInstance = LoadObject<UMaterialInstanceConstant>(nullptr, *PackageName);
    if (ExistingInstance)
    {
        UE_LOG(LogURLabEditor, Log, TEXT("Reusing existing material instance: %s"), *InstanceName);
        return ExistingInstance;
    }

    UE_LOG(LogURLabEditor, Log, TEXT("Creating material instance: %s"), *InstanceName);

    // 创建包
    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();

    // 创建材质实例
    UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>(
        Package,
        FName(*InstanceName),
        RF_Public | RF_Standalone
    );

    MaterialInstance->SetParentEditorOnly(MasterMaterial);

    // 用于设置标量参数并启用覆盖的辅助 lambda 函数
    auto SetScalar = [&](const TCHAR* Name, float Value)
    {
        FMaterialParameterInfo Info(Name);
        MaterialInstance->SetScalarParameterValueEditorOnly(Info, Value);
    };

    // 用于设置向量参数并启用覆盖的辅助 lambda 函数
    auto SetVector = [&](const TCHAR* Name, const FLinearColor& Value)
    {
        FMaterialParameterInfo Info(Name);
        MaterialInstance->SetVectorParameterValueEditorOnly(Info, Value);
    };

    // 用于设置纹理参数的辅助 lambda 函数，并启用覆盖功能。
    auto SetTexture = [&](const TCHAR* Name, UTexture* Tex)
    {
        FMaterialParameterInfo Info(Name);
        MaterialInstance->SetTextureParameterValueEditorOnly(Info, Tex);

        // 同时直接添加到 TextureParameterValues 中，以确保启用覆盖功能。
        FTextureParameterValue TexParam;
        TexParam.ParameterInfo = Info;
        TexParam.ParameterValue = Tex;
        TexParam.ExpressionGUID = FGuid(); // 将由 UE 解决
        MaterialInstance->TextureParameterValues.Add(TexParam);

        UE_LOG(LogURLabEditor, Log, TEXT("  [SetTexture] Set '%s' = '%s' (TextureParameterValues count: %d)"),
            Name, *Tex->GetName(), MaterialInstance->TextureParameterValues.Num());
    };

    // 设置基础颜色（底色）
    SetVector(TEXT("BaseColor"), MaterialData.Rgba);

    // 设置纹理参数
    bool bHasBaseColorTexture = false;
    if (!MaterialData.BaseColorTextureName.IsEmpty())
    {
        if (TextureAssets.Contains(MaterialData.BaseColorTextureName))
        {
            UTexture2D* BaseColorTex = TextureAssets[MaterialData.BaseColorTextureName];
            if (BaseColorTex)
            {
                SetTexture(TEXT("BaseColorTexture"), BaseColorTex);
                bHasBaseColorTexture = true;
                UE_LOG(LogURLabEditor, Log, TEXT("  Texture '%s' applied to material '%s' (tex=%s)"),
                    *MaterialData.BaseColorTextureName, *InstanceName, *BaseColorTex->GetName());
            }
        }
        else
        {
            UE_LOG(LogURLabEditor, Warning, TEXT("  Texture '%s' referenced by material '%s' but not found in imported textures (%d entries)"),
                *MaterialData.BaseColorTextureName, *InstanceName, TextureAssets.Num());
        }
    }

    // 将 bUseTexture 设置为静态开关参数——主材质使用 StaticSwitchParameter 在 false 时完全消除纹理采样分支，防止空纹理崩溃 (UE-23902)。
    {
        FStaticParameterSet StaticParams;
        MaterialInstance->GetStaticParameterValues(StaticParams);

        for (FStaticSwitchParameter& Param : StaticParams.StaticSwitchParameters)
        {
            if (Param.ParameterInfo.Name == TEXT("bUseTexture"))
            {
                Param.Value = bHasBaseColorTexture;
                Param.bOverride = true;
                break;
            }
        }
        MaterialInstance->UpdateStaticPermutation(StaticParams);
    }

    // 如果可用，请设置正则纹理
    if (!MaterialData.NormalTextureName.IsEmpty() && TextureAssets.Contains(MaterialData.NormalTextureName))
    {
        UTexture2D* NormalTex = TextureAssets[MaterialData.NormalTextureName];
        if (NormalTex)
        {
            FMaterialParameterInfo NormalParamInfo(TEXT("NormalTexture"));
            MaterialInstance->SetTextureParameterValueEditorOnly(NormalParamInfo, NormalTex);
        }
    }

    // 如果可用，请设置 ORM 纹理
    // ORM 纹理（也称 ORM 贴图）是一种在 3D 渲染和游戏开发中常用的技术。
    // 它将环境光遮蔽（Occlusion）、粗糙度（Roughness）和金属度（Metallic）这三个 PBR 材质参数合并到一张图像的红、绿、蓝（RGB）通道中，
    // 以减少文件数量并提高渲染效率
    if (!MaterialData.ORMTextureName.IsEmpty() && TextureAssets.Contains(MaterialData.ORMTextureName))
    {
        UTexture2D* ORMTex = TextureAssets[MaterialData.ORMTextureName];
        if (ORMTex)
        {
            FMaterialParameterInfo ORMParamInfo(TEXT("ORMTexture"));
            MaterialInstance->SetTextureParameterValueEditorOnly(ORMParamInfo, ORMTex);
        }
    }
    // 否则，请设置单独的粗糙度/金属纹理。
    else
    {
        if (!MaterialData.RoughnessTextureName.IsEmpty() && TextureAssets.Contains(MaterialData.RoughnessTextureName))
        {
            // 注意：如果主材质没有单独的粗糙度纹理参数，则会忽略此参数。
            // 目前，我们仅将其记录下来。
            UE_LOG(LogURLabEditor, Log, TEXT("Roughness texture found but master material uses ORM workflow"));
        }

        if (!MaterialData.MetallicTextureName.IsEmpty() && TextureAssets.Contains(MaterialData.MetallicTextureName))
        {
            // 注意：如果主材质没有单独的金属纹理参数，则此参数将被忽略。
            UE_LOG(LogURLabEditor, Log, TEXT("Metallic texture found but master material uses ORM workflow"));
        }
    }

    // 强制更新并保存
    MaterialInstance->UpdateStaticPermutation();
    MaterialInstance->PostEditChange();

    // 保存包
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(MaterialInstance);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    // UPackage::SavePackage(Package, MaterialInstance, *PackageFileName, SaveArgs);

    UE_LOG(LogURLabEditor, Log, TEXT("Successfully created material instance: %s"), *InstanceName);
    return MaterialInstance;
}
