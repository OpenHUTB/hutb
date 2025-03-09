// Copyright (c) 2019 Intel Labs.
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla/Sensor/ShaderBasedSensor.h"

#include "Carla/Actor/ActorDefinition.h"

#include "RssSensor.generated.h"

/// RSS传感器表示
/// 实际计算在客户端完成
UCLASS()
class CARLA_API ARssSensor : public ASensor
{
  GENERATED_BODY()

public:

  static FActorDefinition GetSensorDefinition();

  ARssSensor(const FObjectInitializer &ObjectInitializer);

};
