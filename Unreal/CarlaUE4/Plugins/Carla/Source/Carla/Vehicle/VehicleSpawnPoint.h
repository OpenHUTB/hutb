// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once
// 这是一个预处理指令，其作用是确保该头文件在整个编译过程中只会被包含一次，避免因多次包含同一个头文件而导致的编译错误等问题。

#include "Engine/TargetPoint.h"
// 引入 "Engine/TargetPoint.h" 这个头文件，里面应该包含了 ATargetPoint 类的定义等相关内容，意味着当前要定义的类可能会依赖或者继承自这个头文件中所定义的类。
#include "VehicleSpawnPoint.generated.h"
// 包含 "VehicleSpawnPoint.generated.h" 头文件，通常在使用虚幻引擎的反射系统等相关特性时，会有这样一个自动生成代码的头文件，用于配合引擎生成必要的代码辅助类实现各种功能，比如序列化、蓝图相关功能等。

/// 行人生成位置的基类。
// 这是一个 Doxygen 风格的注释（一种常用的代码文档注释规范），用于简要说明下面定义的类的作用，这里表明这个类是用于行人（walkers）生成位置的基类。

UCLASS()
// 使用 UCLASS 宏来声明这是一个虚幻引擎中的类，这个宏会配合引擎做很多和类相关的处理，比如注册类到引擎中，使其能够被正确地实例化、在蓝图中使用等，这里括号内没写额外参数，采用默认的类相关设置。
class CARLA_API AVehicleSpawnPoint : public ATargetPoint
{
  GENERATED_BODY()
 // 这是一个宏，用于告诉虚幻引擎的代码生成工具在这里生成必要的类相关代码体，例如反射相关代码等，以保证类能在虚幻引擎的体系下正常工作，比如支持序列化、在蓝图中显示属性等功能。
};
