// Copyright (c) OpenHUTB. All rights reserved.
//
// ROV 水下物理模型：
//   - FRovOceanCurrent       三层级联洋流
//   - FRovPIDDepthController 定深 PID
//
// 头文件内联实现，不新增 .cpp。

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// =============================================================================
// 洋流：三层级联
//   V_current(x,y,z,t) = V_GaussMarkov(t) + V_stratified(z) + V_turbulent(x,y,z)
// 坐标约定：MuJoCo 世界系，Z 向上，单位 m/s。
// =============================================================================
struct FRovOceanCurrent
{
    /** 分层剖面的一层 */
    struct FLayer
    {
        float DepthM = 0.0f;        // 深度（负值表示水下）
        float Speed = 0.0f;         // 流速幅值 m/s
        float HorizAngle = 0.0f;    // 水平角 rad
        float VertAngle = 0.0f;     // 垂直角 rad（正 = 向下）
    };

    // ---------------------------- 配置 ----------------------------

    bool  bEnabled = true;

    // Gauss-Markov
    float MeanSpeed = 0.5f;
    float MeanHorizAngle = 0.0f;
    float MeanVertAngle = 0.0f;
    float Mu = 0.0f;
    float NoiseAmp = 0.1f;

    // 分层
    TArray<FLayer> Layers;

    // 湍流
    float TurbIntensity = 0.15f;
    float TurbIntegralScale = 1.0f;

    /**
     * 洋流速度幅值上限（m/s）。Gauss-Markov 无均值回归时速度会无界增长，
     * 这里加物理上限。
     */
    float MaxSpeed = 2.0f;

    int32 Seed = 42;

    // ---------------------------- 状态 ----------------------------

    float CurSpeed = 0.0f;
    float CurHoriz = 0.0f;
    float CurVert = 0.0f;

    FVector LastPos = FVector::ZeroVector;
    bool bHasLastPos = false;

    FRandomStream Rng;

    void Initialize()
    {
        Rng.Initialize(Seed);
        if (Layers.Num() == 0) {
            // 与 config.json 的 stratified_layers 一致
            Layers.Add({    0.0f, 0.0f, 0.0f, 0.0f });
            Layers.Add({  -10.0f, 0.5f, 0.3f, 0.0f });
            Layers.Add({  -50.0f, 1.0f, 0.5f, 0.1f });
        }
        // 按深度升序（最深在前）
        Layers.Sort([](const FLayer& A, const FLayer& B) { return A.DepthM < B.DepthM; });
        Reset();
    }

    void Reset()
    {
        CurSpeed = MeanSpeed;
        CurHoriz = MeanHorizAngle;
        CurVert = MeanVertAngle;
        bHasLastPos = false;
        LastPos = FVector::ZeroVector;
    }

    /** Gauss-Markov 层：速度幅值 + 两个角度做均值回归随机游走 */
    FVector UpdateGaussMarkov(float Dt)
    {
        const float SqrtDt = FMath::Sqrt(FMath::Max(Dt, 1e-6f));

        CurSpeed += Mu * (MeanSpeed - CurSpeed) * Dt
                  + NoiseAmp * SqrtDt * Rng.GetFraction() * 2.0f - NoiseAmp * SqrtDt;
        CurSpeed = FMath::Clamp(CurSpeed, 0.0f, MaxSpeed);

        CurHoriz += (Mu * (MeanHorizAngle - CurHoriz)
                   + NoiseAmp * SqrtDt * (Rng.GetFraction() * 2.0f - 1.0f)) * Dt;
        CurVert += (Mu * (MeanVertAngle - CurVert)
                   + NoiseAmp * SqrtDt * (Rng.GetFraction() * 2.0f - 1.0f)) * Dt;

        const float CosV = FMath::Cos(CurVert);
        // NED(Z 向下) → MuJoCo(Z 向上)：vz 取反
        return FVector(
            CurSpeed * CosV * FMath::Cos(CurHoriz),
            CurSpeed * CosV * FMath::Sin(CurHoriz),
            -CurSpeed * FMath::Sin(CurVert));    }

    /** 分层层：按深度线性插值 */
    FVector StratifiedAt(float DepthM) const
    {
        if (Layers.Num() == 0) return FVector::ZeroVector;

        auto LayerVec = [](const FLayer& L) {
            const float CosV = FMath::Cos(L.VertAngle);
            return FVector(
                L.Speed * CosV * FMath::Cos(L.HorizAngle),
                L.Speed * CosV * FMath::Sin(L.HorizAngle),
                -L.Speed * FMath::Sin(L.VertAngle));
        };

        if (DepthM <= Layers[0].DepthM) {
            return LayerVec(Layers[0]);
        }
        for (int32 i = 0; i < Layers.Num() - 1; ++i) {
            if (DepthM >= Layers[i].DepthM && DepthM <= Layers[i + 1].DepthM) {
                const float Span = Layers[i + 1].DepthM - Layers[i].DepthM;
                const float T = (Span > KINDA_SMALL_NUMBER)
                    ? (DepthM - Layers[i].DepthM) / Span : 0.0f;
                return FMath::Lerp(LayerVec(Layers[i]), LayerVec(Layers[i + 1]), T);
            }
        }
        return LayerVec(Layers.Last());
    }

    /** 湍流层：空间相关随机扰动 */
    FVector TurbulentAt(const FVector& Pos)
    {
        float NoiseStrength = TurbIntensity;
        if (bHasLastPos) {
            const float Dist = FVector::Dist(Pos, LastPos);
            const float Correlation = FMath::Exp(-Dist / FMath::Max(TurbIntegralScale, 1e-6f));
            NoiseStrength = TurbIntensity * FMath::Sqrt(FMath::Max(0.0f, 1.0f - Correlation * Correlation));
        }
        LastPos = Pos;
        bHasLastPos = true;

        const FVector Noise(
            Rng.GetFraction() * 2.0f - 1.0f,
            Rng.GetFraction() * 2.0f - 1.0f,
            Rng.GetFraction() * 2.0f - 1.0f);
        return Noise * NoiseStrength * 0.5f;
    }

    /** 总洋流速度（MuJoCo 世界系，m/s） */
    FVector GetVelocity(const FVector& Pos, float Dt)
    {
        if (!bEnabled) return FVector::ZeroVector;

        const FVector VGM = UpdateGaussMarkov(Dt);
        const FVector VStrat = StratifiedAt(-Pos.Z);   // z 向上 → 深度取反
        const FVector VTurb = TurbulentAt(Pos);
        return VGM + VStrat + VTurb;
    }
};

// =============================================================================
// PID 定深控制器
//   F = Kp·e + Ki·∫e·dt + Kd·ė + feedforward·V_current_z
// 坐标约定：Z 向上。正推力 = 向上。
// =============================================================================
struct FRovPIDDepthController
{
    // 增益基于 depth_controller.py 的 Kp=2000 / Ki=50 / Kd=1000，
    // 但那组值是针对 150kg 的模型调的；本工程整机 612.9kg，
    // 积分项积累太慢会留下约 0.5m 的稳态误差，所以 Ki 放大到 500。
    float Kp = 2000.0f;
    float Ki = 500.0f;
    float Kd = 1000.0f;
    float MaxThrust = 5000.0f;
    float FeedforwardGain = 500.0f;

    float IntegralError = 0.0f;
    float PrevError = 0.0f;

    void Reset()
    {
        IntegralError = 0.0f;
        PrevError = 0.0f;
    }

    /**
     * @param TargetZ       目标高度（MuJoCo 世界系，m）
     * @param CurrentZ      当前高度
     * @param Dt            时间步长
     * @param CurrentVz     当前位置的洋流 Z 分量（正 = 向上）
     * @return 需要的 Z 轴推力（N），已限幅
     */
    float Compute(float TargetZ, float CurrentZ, float Dt, float CurrentVz)
    {
        const float Error = TargetZ - CurrentZ;

        IntegralError += Error * Dt;
        IntegralError = FMath::Clamp(IntegralError, -100.0f, 100.0f);   // anti-windup

        const float Derivative = (Dt > KINDA_SMALL_NUMBER) ? (Error - PrevError) / Dt : 0.0f;

        float Thrust = Kp * Error + Ki * IntegralError + Kd * Derivative;

        // 洋流前馈：洋流向上(+vz)时提前减少推力
        Thrust += FeedforwardGain * CurrentVz;

        PrevError = Error;
        return FMath::Clamp(Thrust, -MaxThrust, MaxThrust);
    }
};
