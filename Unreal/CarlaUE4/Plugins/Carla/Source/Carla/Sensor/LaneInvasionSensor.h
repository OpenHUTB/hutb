// Copyright (c) 2019 Intel Labs.
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla/Sensor/ShaderBasedSensor.h"

#include "Carla/Actor/ActorDefinition.h"

#include "LaneInvasionSensor.generated.h"

/// LaneInvasion传感器表示
/// 实际位置计算在客户端完成
UCLASS()
class CARLA_API ALaneInvasionSensor : public ASensor
{
  GENERATED_BODY()

public:

  static FActorDefinition GetSensorDefinition();

  ALaneInvasionSensor(const FObjectInitializer &ObjectInitializer);
};
