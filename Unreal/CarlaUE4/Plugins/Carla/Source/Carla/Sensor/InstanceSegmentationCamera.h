// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla/Sensor/ShaderBasedSensor.h"

#include "Carla/Actor/ActorDefinition.h"

#include "InstanceSegmentationCamera.generated.h"

/// 生成‘实例分割’图像的传感器.
UCLASS()
class CARLA_API AInstanceSegmentationCamera : public AShaderBasedSensor
{
  GENERATED_BODY()

public:

  static FActorDefinition GetSensorDefinition();

  AInstanceSegmentationCamera(const FObjectInitializer &ObjectInitializer);

protected:

  void SetUpSceneCaptureComponent(USceneCaptureComponent2D &SceneCapture) override;
  void PostPhysTick(UWorld *World, ELevelTick TickType, float DeltaSeconds) override;
};
