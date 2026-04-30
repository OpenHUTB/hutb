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
#include "MuJoCo/Components/Controllers/MjKeyframeController.h"
#include "MuJoCo/Core/MjArticulation.h"

DEFINE_LOG_CATEGORY_STATIC(LogMjKeyframes, Log, All);

// ─── Built-in Presets ───

static FMjKeyframePose MakePose(const FString& Name, std::initializer_list<float> Targets,
    float Hold = 1.0f, float Blend = 1.0f)
{
    FMjKeyframePose P;
    P.Name = Name;
    P.Targets.Append(Targets.begin(), Targets.size());
    P.HoldTime = Hold;
    P.BlendTime = Blend;
    return P;
}

static TMap<FString, TArray<FMjKeyframePose>> BuildPresets()
{
    TMap<FString, TArray<FMjKeyframePose>> P;

    // Franka Panda: 7 arm joints + 2 finger joints = 9 actuators
    P.Add(TEXT("Franka: Reach-Grasp-Lift"), {
        MakePose(TEXT("Home"),    {0, -0.3f, 0, -2.0f, 0, 2.0f, 0.78f, 0.04f, 0.04f}, 1.0f, 0.5f),
        MakePose(TEXT("Reach"),   {0, 0.5f, 0, -1.0f, 0, 1.5f, 0.78f, 0.04f, 0.04f}, 0.5f, 1.5f),
        MakePose(TEXT("Grasp"),   {0, 0.5f, 0, -1.0f, 0, 1.5f, 0.78f, 0.0f, 0.0f},   1.0f, 0.3f),
        MakePose(TEXT("Lift"),    {0, -0.3f, 0, -2.0f, 0, 2.0f, 0.78f, 0.0f, 0.0f},   1.0f, 1.5f),
        MakePose(TEXT("Release"), {0, -0.3f, 0, -2.0f, 0, 2.0f, 0.78f, 0.04f, 0.04f}, 0.5f, 0.3f),
    });

    P.Add(TEXT("Franka: Wave"), {
        MakePose(TEXT("Up-Left"),    {0.3f, -0.5f, 0, -1.5f, 0, 2.5f, 0.78f, 0.04f, 0.04f}, 1.5f, 0.5f),
        MakePose(TEXT("Up-Right"),   {-0.3f, -0.5f, 0, -1.5f, 0, 2.5f, 0.78f, 0.04f, 0.04f}, 1.5f, 0.5f),
        MakePose(TEXT("Up-Left 2"),  {0.3f, -0.5f, 0, -1.5f, 0, 2.5f, 0.78f, 0.04f, 0.04f}, 1.5f, 0.5f),
        MakePose(TEXT("Up-Right 2"), {-0.3f, -0.5f, 0, -1.5f, 0, 2.5f, 0.78f, 0.04f, 0.04f}, 1.5f, 0.5f),
    });

    // UR5e: 6 revolute joints = 6 actuators (slower blend for smooth industrial motion)
    P.Add(TEXT("UR5e: Pick-Place"), {
        MakePose(TEXT("Home"),    {0, -1.57f, 1.57f, -1.57f, -1.57f, 0},         2.5f, 1.0f),
        MakePose(TEXT("Reach"),   {0.5f, -1.0f, 1.2f, -1.8f, -1.57f, 0.5f},     2.0f, 1.5f),
        MakePose(TEXT("Retract"), {-0.5f, -1.57f, 1.57f, -1.57f, -1.57f, -0.5f}, 2.5f, 1.0f),
    });

    P.Add(TEXT("UR5e: Sweep"), {
        MakePose(TEXT("Left"),   {-1.0f, -1.2f, 1.8f, -2.0f, -1.57f, 0},  2.0f, 1.5f),
        MakePose(TEXT("Center"), {0, -1.2f, 1.8f, -2.0f, -1.57f, 0},      2.0f, 1.5f),
        MakePose(TEXT("Right"),  {1.0f, -1.2f, 1.8f, -2.0f, -1.57f, 0},   2.0f, 1.5f),
        MakePose(TEXT("Center"), {0, -1.2f, 1.8f, -2.0f, -1.57f, 0},      2.0f, 1.5f),
    });

    // ========== SPOT ==========
    // Actuator order: fl_hx,fl_hy,fl_kn, fr_hx,fr_hy,fr_kn, hl_hx,hl_hy,hl_kn, hr_hx,hr_hy,hr_kn
    // Home: [0, 1.04, -1.8] per leg

    P.Add(TEXT("Spot: FR Knee Cycle"), {
        MakePose(TEXT("Home"),   {0,1.04f,-1.8f, 0,1.04f,-1.8f,   0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.5f, 0.5f),
        MakePose(TEXT("Extend"), {0,1.04f,-1.8f, 0,1.04f,-0.248f, 0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.5f, 0.5f),
        MakePose(TEXT("Home 2"), {0,1.04f,-1.8f, 0,1.04f,-1.8f,   0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.5f, 0.3f),
        MakePose(TEXT("Flex"),   {0,1.04f,-1.8f, 0,1.04f,-2.79f,  0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.5f, 0.5f),
        MakePose(TEXT("Home 3"), {0,1.04f,-1.8f, 0,1.04f,-1.8f,   0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.5f, 0.3f),
    });

    P.Add(TEXT("Spot: Wave"), {
        MakePose(TEXT("Stand"),    {0,1.04f,-1.8f,  0,1.04f,-1.8f,  0,1.04f,-1.8f, 0,1.04f,-1.8f},  1.5f, 0.5f),
        MakePose(TEXT("Paw Up"),   {0,1.04f,-1.8f,  0,0.3f,-0.5f,   0,1.04f,-1.8f, 0,1.04f,-1.8f},  1.5f, 0.3f),
        MakePose(TEXT("Wave L"),   {0,1.04f,-1.8f,  0.3f,0.3f,-0.5f, 0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.0f, 0.2f),
        MakePose(TEXT("Wave R"),   {0,1.04f,-1.8f, -0.3f,0.3f,-0.5f, 0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.0f, 0.2f),
        MakePose(TEXT("Wave L2"),  {0,1.04f,-1.8f,  0.3f,0.3f,-0.5f, 0,1.04f,-1.8f, 0,1.04f,-1.8f}, 1.0f, 0.2f),
        MakePose(TEXT("Paw Down"), {0,1.04f,-1.8f,  0,1.04f,-1.8f,  0,1.04f,-1.8f, 0,1.04f,-1.8f},  1.5f, 0.5f),
    });

    P.Add(TEXT("Spot: Sit (Hold)"), {
        MakePose(TEXT("Sit"), {0,0.8f,-1.5f, 0,0.8f,-1.5f, 0,2.0f,-2.5f, 0,2.0f,-2.5f}, 2.5f, 999.0f),
    });

    // ========== ANYMAL ==========
    // Actuator order: LF_HAA,LF_HFE,LF_KFE, RF_HAA,RF_HFE,RF_KFE, LH_HAA,LH_HFE,LH_KFE, RH_HAA,RH_HFE,RH_KFE
    // Standing home: ~[0, 0.5, -1.0] per leg

    // ANYmal: gentle sit/hold — hind legs fold, front legs stay planted
    P.Add(TEXT("ANYmal: Sit (Hold)"), {
        MakePose(TEXT("Sit"), {0,0.4f,-0.8f, 0,0.4f,-0.8f, 0,1.0f,-1.8f, 0,1.0f,-1.8f}, 2.5f, 999.0f),
    });

    // ANYmal: slow gentle look-around — small hip abduction shifts weight side to side
    P.Add(TEXT("ANYmal: Weight Shift"), {
        MakePose(TEXT("Stand"),   {0,0.5f,-1.0f,    0,0.5f,-1.0f,    0,0.5f,-1.0f,    0,0.5f,-1.0f},    2.0f, 1.0f),
        MakePose(TEXT("Lean L"),  {-0.15f,0.5f,-1.0f, 0.15f,0.5f,-1.0f, -0.15f,0.5f,-1.0f, 0.15f,0.5f,-1.0f}, 2.0f, 1.0f),
        MakePose(TEXT("Center"),  {0,0.5f,-1.0f,    0,0.5f,-1.0f,    0,0.5f,-1.0f,    0,0.5f,-1.0f},    2.0f, 0.5f),
        MakePose(TEXT("Lean R"),  {0.15f,0.5f,-1.0f, -0.15f,0.5f,-1.0f, 0.15f,0.5f,-1.0f, -0.15f,0.5f,-1.0f}, 2.0f, 1.0f),
        MakePose(TEXT("Center2"), {0,0.5f,-1.0f,    0,0.5f,-1.0f,    0,0.5f,-1.0f,    0,0.5f,-1.0f},    2.0f, 0.5f),
    });

    // ========== GO2 ==========
    // Actuator order: FL_hip,FL_thigh,FL_calf, FR_hip,FR_thigh,FR_calf, RL_hip,RL_thigh,RL_calf, RR_hip,RR_thigh,RR_calf
    // Home from keyframe: [0, 0.9, -1.8] per leg

    P.Add(TEXT("Go2: Stand-Crouch"), {
        MakePose(TEXT("Stand"),  {0,0.9f,-1.8f, 0,0.9f,-1.8f, 0,0.9f,-1.8f, 0,0.9f,-1.8f},  2.0f, 1.0f),
        MakePose(TEXT("Crouch"), {0,1.5f,-2.5f, 0,1.5f,-2.5f, 0,1.5f,-2.5f, 0,1.5f,-2.5f},  2.0f, 1.0f),
        MakePose(TEXT("Stand2"), {0,0.9f,-1.8f, 0,0.9f,-1.8f, 0,0.9f,-1.8f, 0,0.9f,-1.8f},  2.0f, 0.5f),
    });

    P.Add(TEXT("Go2: Stretch"), {
        MakePose(TEXT("Stand"),  {0,0.9f,-1.8f,  0,0.9f,-1.8f,  0,0.9f,-1.8f,  0,0.9f,-1.8f},  1.5f, 0.5f),
        MakePose(TEXT("Front"),  {0,0.4f,-0.9f,  0,0.4f,-0.9f,  0,1.5f,-2.5f,  0,1.5f,-2.5f},  2.0f, 1.0f),
        MakePose(TEXT("Stand2"), {0,0.9f,-1.8f,  0,0.9f,-1.8f,  0,0.9f,-1.8f,  0,0.9f,-1.8f},  2.0f, 0.5f),
        MakePose(TEXT("Back"),   {0,1.5f,-2.5f,  0,1.5f,-2.5f,  0,0.4f,-0.9f,  0,0.4f,-0.9f},  2.0f, 1.0f),
        MakePose(TEXT("Stand3"), {0,0.9f,-1.8f,  0,0.9f,-1.8f,  0,0.9f,-1.8f,  0,0.9f,-1.8f},  2.0f, 0.5f),
    });

    return P;
}

static const TMap<FString, TArray<FMjKeyframePose>>& GetAllPresets()
{
    static TMap<FString, TArray<FMjKeyframePose>> Presets = BuildPresets();
    return Presets;
}

// ─── Implementation ───

UMjKeyframeController::UMjKeyframeController()
{
    PrimaryComponentTick.bCanEverTick = true;
}

TArray<FString> UMjKeyframeController::GetPresetNames() const
{
    TArray<FString> Names;
    const auto& Presets = GetAllPresets();
    Presets.GetKeys(Names);
    Names.Sort();
    UE_LOG(LogMjKeyframes, Verbose, TEXT("GetPresetNames: %d presets available"), Names.Num());
    for (const auto& N : Names)
    {
        UE_LOG(LogMjKeyframes, Verbose, TEXT("  - '%s'"), *N);
    }
    return Names;
}

void UMjKeyframeController::LoadPreset(const FString& PresetName)
{
    UE_LOG(LogMjKeyframes, Verbose, TEXT("LoadPreset called with '%s'"), *PresetName);

    const auto& Presets = GetAllPresets();
    UE_LOG(LogMjKeyframes, Verbose, TEXT("  Available presets: %d"), Presets.Num());
    for (const auto& Pair : Presets)
    {
        UE_LOG(LogMjKeyframes, Verbose, TEXT("    '%s' -> %d keyframes"), *Pair.Key, Pair.Value.Num());
    }

    if (const auto* Found = Presets.Find(PresetName))
    {
        Keyframes = *Found;
        UE_LOG(LogMjKeyframes, Verbose, TEXT("  Loaded preset '%s' -> %d keyframes"), *PresetName, Keyframes.Num());
        for (int32 i = 0; i < Keyframes.Num(); ++i)
        {
            const auto& KF = Keyframes[i];
            FString TargetsStr;
            for (float T : KF.Targets)
            {
                TargetsStr += FString::Printf(TEXT("%.2f, "), T);
            }
            UE_LOG(LogMjKeyframes, Verbose, TEXT("    [%d] '%s': targets=[%s] hold=%.1f blend=%.1f"),
                i, *KF.Name, *TargetsStr, KF.HoldTime, KF.BlendTime);
        }
    }
    else
    {
        UE_LOG(LogMjKeyframes, Warning, TEXT("  Preset '%s' NOT FOUND in %d presets"),
            *PresetName, Presets.Num());
    }
}

AMjArticulation* UMjKeyframeController::ResolveOwnerArticulation()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogMjKeyframes, Warning, TEXT("ResolveOwnerArticulation: No owner actor"));
        return nullptr;
    }

    AMjArticulation* Artic = Cast<AMjArticulation>(Owner);
    if (Artic)
    {
        UE_LOG(LogMjKeyframes, Log, TEXT("Resolved owner articulation: '%s'"), *Artic->GetName());
        return Artic;
    }

    UE_LOG(LogMjKeyframes, Warning, TEXT("Owner '%s' is not an AMjArticulation"), *Owner->GetName());
    return nullptr;
}

void UMjKeyframeController::BeginPlay()
{
    Super::BeginPlay();

    OwnerArticulation = ResolveOwnerArticulation();

    UE_LOG(LogMjKeyframes, Log, TEXT("BeginPlay: Preset='%s', Keyframes=%d, AutoPlay=%s, OwnerArticulation=%s"),
        *Preset, Keyframes.Num(),
        bAutoPlay ? TEXT("true") : TEXT("false"),
        OwnerArticulation ? *OwnerArticulation->GetName() : TEXT("NULL"));

    // Auto-load preset if set and keyframes are empty
    if (!Preset.IsEmpty() && Keyframes.Num() == 0)
    {
        UE_LOG(LogMjKeyframes, Log, TEXT("  Auto-loading preset '%s'"), *Preset);
        LoadPreset(Preset);
    }
    else if (!Preset.IsEmpty() && Keyframes.Num() > 0)
    {
        UE_LOG(LogMjKeyframes, Log, TEXT("  Preset set but keyframes already populated (%d), skipping auto-load"), Keyframes.Num());
    }

    if (bAutoPlay)
    {
        bDelayPending = true;
        DelayTimer = StartDelay;
        UE_LOG(LogMjKeyframes, Log, TEXT("  AutoPlay scheduled with %.1fs delay"), StartDelay);
    }
}

void UMjKeyframeController::CacheActuatorNames()
{
    if (!OwnerArticulation) return;
    ActuatorNames = OwnerArticulation->GetActuatorNames();
    UE_LOG(LogMjKeyframes, Verbose, TEXT("Cached %d actuator names from '%s'"),
        ActuatorNames.Num(), *OwnerArticulation->GetName());
    for (int32 i = 0; i < ActuatorNames.Num(); ++i)
    {
        UE_LOG(LogMjKeyframes, Verbose, TEXT("  [%d] %s"), i, *ActuatorNames[i]);
    }
}

void UMjKeyframeController::Play()
{
    if (Keyframes.Num() == 0)
    {
        UE_LOG(LogMjKeyframes, Warning, TEXT("No keyframes defined"));
        return;
    }
    if (!OwnerArticulation)
    {
        OwnerArticulation = ResolveOwnerArticulation();
        if (!OwnerArticulation)
        {
            UE_LOG(LogMjKeyframes, Warning, TEXT("Owner is not an MjArticulation — add this component to an MjArticulation actor"));
            return;
        }
    }

    CacheActuatorNames();
    CurrentKeyframe = 0;
    PhaseTimer = 0.0f;
    bPlaying = true;
    UE_LOG(LogMjKeyframes, Log, TEXT("Playing %d keyframes on '%s'"),
        Keyframes.Num(), *OwnerArticulation->GetName());
}

void UMjKeyframeController::Stop()
{
    bPlaying = false;
}

void UMjKeyframeController::GoToKeyframe(int32 Index)
{
    if (!OwnerArticulation || Keyframes.Num() == 0) return;
    if (ActuatorNames.Num() == 0) CacheActuatorNames();

    Index = FMath::Clamp(Index, 0, Keyframes.Num() - 1);
    const FMjKeyframePose& Pose = Keyframes[Index];

    int32 Count = FMath::Min(Pose.Targets.Num(), ActuatorNames.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        OwnerArticulation->SetActuatorControl(ActuatorNames[i], Pose.Targets[i]);
    }
}

void UMjKeyframeController::ApplyInterpolatedTargets(
    const TArray<float>& From, const TArray<float>& To, float Alpha)
{
    if (!OwnerArticulation || ActuatorNames.Num() == 0) return;

    int32 Count = FMath::Min3(From.Num(), To.Num(), ActuatorNames.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        float Val = FMath::Lerp(From[i], To[i], Alpha);
        OwnerArticulation->SetActuatorControl(ActuatorNames[i], Val);
    }
}

void UMjKeyframeController::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Handle preset loading from Details panel
    if (bLoadPreset)
    {
        bLoadPreset = false;
        UE_LOG(LogMjKeyframes, Log, TEXT("bLoadPreset triggered, Preset='%s'"), *Preset);
        if (!Preset.IsEmpty())
        {
            LoadPreset(Preset);
        }
        else
        {
            UE_LOG(LogMjKeyframes, Warning, TEXT("bLoadPreset ticked but Preset string is empty"));
        }
    }

    // Handle start delay
    if (bDelayPending)
    {
        DelayTimer -= DeltaTime;
        if (DelayTimer <= 0.0f)
        {
            bDelayPending = false;
            Play();
        }
        return;
    }

    if (!bPlaying || Keyframes.Num() == 0 || !OwnerArticulation) return;

    const FMjKeyframePose& Current = Keyframes[CurrentKeyframe];
    float TotalPhaseTime = Current.BlendTime + Current.HoldTime;

    PhaseTimer += DeltaTime;

    if (PhaseTimer < Current.BlendTime)
    {
        // Blending from previous pose to current
        float Alpha = Current.BlendTime > 0.0f
            ? FMath::Clamp(PhaseTimer / Current.BlendTime, 0.0f, 1.0f)
            : 1.0f;

        // Smooth ease-in-out
        Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

        int32 PrevIdx = (CurrentKeyframe > 0)
            ? CurrentKeyframe - 1
            : (bLoop ? Keyframes.Num() - 1 : 0);

        ApplyInterpolatedTargets(Keyframes[PrevIdx].Targets, Current.Targets, Alpha);
    }
    else if (PhaseTimer < TotalPhaseTime)
    {
        // Holding at current pose
        GoToKeyframe(CurrentKeyframe);
    }
    else
    {
        // Advance to next keyframe
        int32 OldKF = CurrentKeyframe;
        CurrentKeyframe++;
        PhaseTimer = 0.0f;

        if (CurrentKeyframe >= Keyframes.Num())
        {
            if (bLoop)
            {
                CurrentKeyframe = 0;
                UE_LOG(LogMjKeyframes, Log, TEXT("Looping: keyframe %d -> 0 ('%s')"),
                    OldKF, *Keyframes[0].Name);
            }
            else
            {
                CurrentKeyframe = Keyframes.Num() - 1;
                bPlaying = false;
                UE_LOG(LogMjKeyframes, Log, TEXT("Keyframe playback complete"));
            }
        }
        else
        {
            UE_LOG(LogMjKeyframes, Log, TEXT("Advancing: keyframe %d '%s' -> %d '%s'"),
                OldKF, *Keyframes[OldKF].Name,
                CurrentKeyframe, *Keyframes[CurrentKeyframe].Name);
        }
    }
}
