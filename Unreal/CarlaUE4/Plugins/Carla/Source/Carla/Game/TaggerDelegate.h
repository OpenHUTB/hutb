// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

// 预处理指令，确保该头文件在整个项目中只被包含一次，避免重复定义等问题
#pragma once 

// 包含引擎世界相关的头文件，用于获取和操作游戏世界中的各种对象、资源等信息，是UE4中与世界场景交互的重要头文件
#include "Engine/World.h" 

// 表明这个类是由虚幻引擎的反射系统生成代码的，用于实现诸如蓝图可视化、序列化等功能
#include "TaggerDelegate.generated.h" 

// UTaggerDelegate类的注释说明，表明这个类的作用是用于给在世界中生成的每个角色（Actor）添加标签（从语义上理解，可能是做一些标识或者分类相关操作）
/// 用于给生成到世界中的每个 Actor 添加标签。
// 定义一个名为UTaggerDelegate的类，它继承自UObject，所在的命名空间为CARLA_API（可能是特定项目或模块定义的命名空间）
UCLASS()
class CARLA_API UTaggerDelegate : public UObject
{
    // 这是UE4的宏，用于生成类的必要代码结构，和反射相关机制配合使用，一般不需要手动修改这部分内容
    GENERATED_BODY()

public:
    // 构造函数，用于初始化UTaggerDelegate类的对象，通常在这里进行一些成员变量的初始化等基础设置工作
    UTaggerDelegate(); 

    // 函数用于向给定的游戏世界（UWorld类型指针表示）注册一个生成处理程序（Spawn Handler），具体的注册逻辑应该在函数内部实现，目的可能是为了能捕获世界中角色生成的事件等
    void RegisterSpawnHandler(UWorld *World); 

    // 函数用于设置语义分割（Semantic Segmentation）是否启用，有一个默认参数为true，表示如果不传入参数调用该函数时默认启用语义分割，通过修改内部的布尔变量bSemanticSegmentationEnabled来控制状态
    void SetSemanticSegmentationEnabled(bool Enable = true)
    {
        bSemanticSegmentationEnabled = Enable;
    }

    // 函数在角色（AActor类型指针表示）在世界中被生成时会被调用，应该是用于执行对生成角色进行添加标签或者其他相关处理的逻辑，具体逻辑需要在函数内部实现
    void OnActorSpawned(AActor *Actor); 

private:
    // 定义一个委托（Delegate）类型的成员变量，具体类型是FOnActorSpawned::FDelegate，用于绑定和响应角色生成相关的事件，当角色在世界中生成时，与之绑定的函数会被调用，以此来实现一些自定义的逻辑处理
    FOnActorSpawned::FDelegate ActorSpawnedDelegate; 

    // 一个布尔类型的成员变量，用于记录语义分割是否启用的状态，初始化为false，可通过SetSemanticSegmentationEnabled函数来修改其值
    bool bSemanticSegmentationEnabled = false; 
};
