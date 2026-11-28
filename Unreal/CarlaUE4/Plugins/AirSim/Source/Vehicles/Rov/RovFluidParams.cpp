// Copyright (c) OpenHUTB. All rights reserved.

#include "Vehicles/Rov/RovFluidParams.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FShapeParams
{
    float Cd;
    float AddedMassCoeff; // 相对 ρ·V 的附加质量系数
};

FShapeParams ShapeTable(ERovHullShape Shape)
{
    switch (Shape) {
    case ERovHullShape::Sphere:
        return { 0.47f, 0.50f };
    case ERovHullShape::Box:
        return { 1.05f, 0.70f };
    case ERovHullShape::Cylinder:
        return { 0.82f, 0.60f };
    case ERovHullShape::Streamlined:
        return { 0.08f, 0.10f };
    default:
        return { 0.50f, 0.50f };
    }
}
}

FRovFluidEstimate URovFluidLibrary::EstimateFluidParams(ERovHullShape Shape, FVector Dims, float Density)
{
    FRovFluidEstimate Out;
    const FShapeParams P = ShapeTable(Shape);
    Out.Cd = P.Cd;

    const float X = FMath::Max(Dims.X, KINDA_SMALL_NUMBER);
    const float Y = FMath::Max(Dims.Y, KINDA_SMALL_NUMBER);
    const float Z = FMath::Max(Dims.Z, KINDA_SMALL_NUMBER);

    switch (Shape) {
    case ERovHullShape::Sphere:
        Out.Volume = 4.0f / 3.0f * PI * X * X * X;
        Out.RefArea = PI * X * X;
        break;
    case ERovHullShape::Box:
        Out.Volume = 8.0f * X * Y * Z;
        Out.RefArea = 4.0f * Y * Z; // 沿 X 迎流
        break;
    case ERovHullShape::Cylinder:
        Out.Volume = PI * X * X * 2.0f * Y;
        Out.RefArea = PI * X * X;
        break;
    case ERovHullShape::Streamlined:
        Out.Volume = 4.0f / 3.0f * PI * X * Y * Z;
        Out.RefArea = PI * Y * Z; // 沿长轴 X 迎流
        break;
    default:
        break;
    }

    Out.AddedMass = P.AddedMassCoeff * Density * Out.Volume;
    return Out;
}

float URovFluidLibrary::EstimateDragForce(ERovHullShape Shape, FVector Dims, float Speed, float Density)
{
    const FRovFluidEstimate E = EstimateFluidParams(Shape, Dims, Density);
    return 0.5f * Density * E.Cd * E.RefArea * Speed * Speed;
}

bool RovConfig::LoadKeyValues(const FString& Path, TMap<FString, float>& OutValues, FString& OutError)
{
    OutValues.Reset();
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path)) {
        OutError = FString::Printf(TEXT("读取失败：%s"), *Path);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        OutError = FString::Printf(TEXT("JSON 解析失败：%s"), *Path);
        return false;
    }

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values) {
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Number) {
            continue;
        }
        OutValues.Add(Pair.Key, static_cast<float>(Pair.Value->AsNumber()));
    }
    return true;
}

bool RovConfig::LoadNumberArray(const FString& Path, const FString& Field, TArray<float>& OutValues, FString& OutError)
{
    OutValues.Reset();
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path)) {
        OutError = FString::Printf(TEXT("读取失败：%s"), *Path);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) {
        OutError = FString::Printf(TEXT("JSON 解析失败：%s"), *Path);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (!Root->TryGetArrayField(Field, Arr) || !Arr) {
        OutError = FString::Printf(TEXT("缺少数组字段 %s"), *Field);
        return false;
    }
    for (const TSharedPtr<FJsonValue>& V : *Arr) {
        if (V.IsValid() && V->Type == EJson::Number) {
            OutValues.Add(static_cast<float>(V->AsNumber()));
        }
    }
    return true;
}
