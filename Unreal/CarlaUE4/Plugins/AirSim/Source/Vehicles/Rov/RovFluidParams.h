// Copyright (c) OpenHUTB. All rights reserved.
//
// ROV 流体参数工具：外形 → 阻力系数估算、配置文件读取。

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RovFluidParams.generated.h"

/** 典型外形。 */
UENUM(BlueprintType)
enum class ERovHullShape : uint8
{
    Sphere UMETA(DisplayName = "球体"),
    Box UMETA(DisplayName = "立方体/长方体"),
    Cylinder UMETA(DisplayName = "圆柱"),
    Streamlined UMETA(DisplayName = "流线型"),
};

/** 由外形估算出的流体受力参数。 */
USTRUCT(BlueprintType)
struct FRovFluidEstimate
{
    GENERATED_BODY()

    /** 迎流阻力系数 Cd。 */
    UPROPERTY(BlueprintReadOnly, Category = "ROV|Fluid")
    float Cd = 0.5f;

    /** 迎流参考面积（m²）。 */
    UPROPERTY(BlueprintReadOnly, Category = "ROV|Fluid")
    float RefArea = 0.0f;

    /** 附加质量（kg）。 */
    UPROPERTY(BlueprintReadOnly, Category = "ROV|Fluid")
    float AddedMass = 0.0f;

    /** 排水体积（m³）。 */
    UPROPERTY(BlueprintReadOnly, Category = "ROV|Fluid")
    float Volume = 0.0f;
};

/**
 * 外形到流体参数的估算。
 *
 * 尺寸约定（单位 m，均为半尺寸）：
 *   球体      X = 半径
 *   立方体    X/Y/Z = 三个方向的半边长
 *   圆柱      X = 半径，Y = 半长（轴向迎流）
 *   流线型    X/Y/Z = 椭球三个半轴，X 为长轴（长轴迎流）
 */
UCLASS()
class AIRSIM_API URovFluidLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 由外形与尺寸估算 Cd、迎流面积、附加质量。 */
    UFUNCTION(BlueprintCallable, Category = "ROV|Fluid")
    static FRovFluidEstimate EstimateFluidParams(ERovHullShape Shape, FVector Dims, float Density = 1000.0f);

    /** 按估算的 Cd 计算匀速迎流阻力 F = 0.5·ρ·Cd·A·v²（N）。 */
    UFUNCTION(BlueprintCallable, Category = "ROV|Fluid")
    static float EstimateDragForce(ERovHullShape Shape, FVector Dims, float Speed, float Density = 1000.0f);
};

/** 配置文件读取。 */
namespace RovConfig
{
/**
     * 读取扁平 JSON 配置文件（键为字符串，值为数值）。
     * 数组与嵌套对象会被忽略，由调用方单独解析。
     */
bool LoadKeyValues(const FString& Path, TMap<FString, float>& OutValues, FString& OutError);

/** 读取配置文件里的数值数组字段（如 "Obstacles" 之外的一维数组）。 */
bool LoadNumberArray(const FString& Path, const FString& Field, TArray<float>& OutValues, FString& OutError);
}
