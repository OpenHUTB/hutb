// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
// 上面两行注释意思是版权所有 (c) 2017 巴塞罗那自治大学（UAB）计算机视觉中心（CVC）
//
// This work is licensed under the terms of the MIT license.（这项工作是根据MIT许可证的条款授权的）
// For a copy, see <https://opensource.org/licenses/MIT>.（要获取副本，请前往<https://opensource.org/licenses/MIT>）

#pragma once  // 确保头文件内容在每个编译单元中只被包含一次

// 包含禁用虚幻引擎4（UE4）宏的头文件
#include <compiler/disable-ue4-macros.h>

// 包含CARLA RPC中定义的角色属性类型的头文件
#include <carla/rpc/ActorAttributeType.h> 

// 包含启用虚幻引擎4（UE4）宏的头文件
#include <compiler/enable-ue4-macros.h>

 // 包含自动生成的头文件，通常用于Unreal Engine 4 (UE4) 中的反射系统
#include "ActorAttribute.generated.h" 

// 定义一个宏，用于将carla::rpc::ActorAttributeType枚举值转换为uint8类型
#define CARLA_ENUM_FROM_RPC(e) static_cast<uint8>(carla::rpc::ActorAttributeType::e)

/// List of valid types for actor attributes.（角色属性有效类型的列表）

// 标记这个枚举可以在Unreal Engine的蓝图系统中使用
UENUM(BlueprintType)
enum class EActorAttributeType : uint8
{
  Null = 0, // Workarround for UE4.24 issue with enums（针对UE4.24中枚举问题的临时解决方案）
  Bool      = CARLA_ENUM_FROM_RPC(Bool)      UMETA(DisplayName = "Bool"),  // 布尔类型
  Int       = CARLA_ENUM_FROM_RPC(Int)       UMETA(DisplayName = "Integer"),  // 整数类型
  Float     = CARLA_ENUM_FROM_RPC(Float)     UMETA(DisplayName = "Float"),  // 浮点数类型
  String    = CARLA_ENUM_FROM_RPC(String)    UMETA(DisplayName = "String"),  // 字符串类型
  RGBColor  = CARLA_ENUM_FROM_RPC(RGBColor)  UMETA(DisplayName = "RGB Color (comma separated)"),  // RGB颜色类型（逗号分隔）
 
  SIZE        UMETA(Hidden),  // 隐藏属性，不应在UI中显示
  INVALID     UMETA(Hidden)  // 无效属性，不应在UI中显示
};

#undef CARLA_ENUM_FROM_RPC// 取消定义宏

/// Definition of an actor variation. Variations represent attributes of the
/// actor that the user can modify in order to generate variations of the same
/// actor.
/// 上面的代码意思为角色变体的定义。变体表示用户可以修改的角色属性，以生成相同角色的变体
///
/// A list of recommended values is given. If bRestrictToRecommended is true,
/// only recommended values are accepted as valid.
/// 上面的代码意思为提供了一组推荐值。如果bRestrictToRecommended为true，则只接受推荐值作为有效值

// 标记这个结构体可以在Unreal Engine的蓝图系统中使用
USTRUCT(BlueprintType)
struct CARLA_API FActorVariation
{
  GENERATED_BODY()  // 自动生成反射代码
 
  UPROPERTY(EditAnywhere, BlueprintReadWrite)  // 标记这个属性可以在任何位置编辑，并且可以在蓝图中读写
  FString Id;  // 变体标识符
 
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  EActorAttributeType Type = EActorAttributeType::String;  // 变体类型
 
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  TArray<FString> RecommendedValues;  // 推荐值列表
 
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  bool bRestrictToRecommended = false;  // 是否限制为只能使用推荐值
};

/// 角色属性，可以是角色的固有（不可修改）属性，也可以是用户自定义的角色变体。
/// actor or an user-defined actor variation.
/// 上面的代码意思为角色属性，可以是角色的固有（不可修改）属性或用户定义的角色变体

// 标记这个结构体可以在Unreal Engine的蓝图系统中使用
USTRUCT(BlueprintType)
struct CARLA_API FActorAttribute//FActorAttribute用于表示或者管理模拟器中actor的某个属性。
{
  GENERATED_BODY()  // 自动生成反射代码
 
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  FString Id;  // 属性标识符
 
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  EActorAttributeType Type = EActorAttributeType::String;  // 属性类型
 
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  FString Value;  // 属性值
};
