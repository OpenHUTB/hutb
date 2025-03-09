// Copyright (c) 2020 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// 版权声明，表明该代码受 MIT 许可证条款的许可，如需查看许可证内容，可通过给定的网址访问
// For a copy, see <https://opensource.org/licenses/MIT>.

// 预编译指令，确保头文件在多次被包含时只被编译一次，避免重复定义等问题
#pragma once

// 引入 "ProceduralCustomMesh.generated.h" 头文件，这是一个由虚幻引擎的代码生成工具生成的头文件，
// 通常包含了与当前结构体相关的反射等元数据信息，对于结构体能够在虚幻引擎中被正确识别、序列化、蓝图调用等操作起到关键作用
#include "ProceduralCustomMesh.generated.h"

// 使用 USTRUCT(Blueprintable) 宏定义，这是虚幻引擎中用于标记一个结构体可以被引擎的蓝图系统使用的宏，
// 意味着该结构体可以在蓝图中进行实例化、访问其成员等操作，方便在虚幻引擎的可视化编程（蓝图）环境中使用
/// Carla 网格的定义。
// 这里是一个简单的注释说明，表明这个结构体是对 Carla 网格（Mesh）的一种定义，后续结构体中的成员变量大概率是用于描述网格的各种属性
USTRUCT(Blueprintable)
struct CARLA_API FProceduralCustomMesh
{
    // 使用 GENERATED_BODY() 宏，这是虚幻引擎要求的，用于指示编译器在这里生成与反射、序列化等相关的必要代码，
    // 保证结构体在引擎中的各种特性能够正常工作
    GENERATED_BODY()

    // 使用 UPROPERTY 宏定义了一个成员变量 Vertices 的属性，使其可以在蓝图中进行读写操作（BlueprintReadWrite），
    // 并且可以在虚幻引擎的编辑器中随意编辑（EditAnywhere），所属的分类为 "VertexData"，方便在编辑器界面中归类显示。
    // Vertices 成员变量的类型是 TArray<FVector>，即一个由 FVector 类型元素组成的数组，在虚幻引擎中，FVector 通常用于表示三维空间中的向量，
    // 这里用于存储网格的顶点坐标信息，也就是构成这个网格的各个顶点在三维空间中的位置
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="VertexData")
    TArray<FVector> Vertices;

    // 同样使用 UPROPERTY 宏定义了 Triangles 成员变量的属性，具备和 Vertices 类似的在蓝图及编辑器中的可操作性（BlueprintReadWrite、EditAnywhere，所属分类 "VertexData"）。
    // 其类型是 TArray<int32>，是一个由 32 位整数组成的数组，在图形学中，通常用于表示构成网格表面三角形的索引信息，
    // 例如每个三个连续的整数可以表示一个三角形的三个顶点在 Vertices 数组中的索引，通过这种方式来描述网格的三角面构成情况
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="VertexData")
    TArray<int32> Triangles;

    // 该 UPROPERTY 宏定义的 Normals 成员变量，同样具有在蓝图和编辑器中相应的读写、编辑属性（BlueprintReadWrite、EditAnywhere，分类 "VertexData"）。
    // 其类型为 TArray<FVector>，也是由 FVector 类型元素组成的数组，这里的 FVector 表示的是每个顶点对应的法线向量，
    // 法线向量用于描述顶点所在表面的朝向，在光照计算、碰撞检测等很多图形学相关的操作中都起着重要作用，用于定义网格表面的几何特性
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="VertexData")
    TArray<FVector> Normals;

    // 对于 UV0 成员变量，通过 UPROPERTY 宏赋予其在蓝图可读写、编辑器可编辑（BlueprintReadWrite、EditAnywhere，分类 "VertexData"）的属性。
    // 它的类型是 TArray<FVector2D>，是由二维向量（FVector2D）组成的数组，在图形学中，UV 坐标（纹理坐标）用于将二维纹理映射到三维模型表面，
    // 这里的 UV0 大概率是存储了网格顶点对应的第一套纹理坐标信息，用于指定纹理在模型上的铺贴方式等操作
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="VertexData")
    TArray<FVector2D> UV0;

    // 这个 UPROPERTY 宏定义的 VertexColor 成员变量，同样具备在蓝图及编辑器中的相应操作属性（BlueprintReadWrite、EditAnywhere，分类 "VertexData"）。
    // 其类型为 TArray<FLinearColor>，是由 FLinearColor 类型元素组成的数组，在虚幻引擎中，FLinearColor 用于表示线性颜色空间下的颜色值，
    // 这里用于存储每个顶点的颜色信息，可用于实现顶点颜色绘制等功能，比如给网格的不同顶点赋予不同颜色来实现特定的视觉效果
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="VertexData")
    TArray<FLinearColor> VertexColor;

    // 以下这部分代码被注释掉了，注释说明提到是因为包含 ProceduralMeshComponent.h 时出现了一个奇怪的 bug。
    // 这里原本定义的 Tangents 成员变量，其类型是 TArray<FProcMeshTangent>，推测在未出现问题时，它可能用于存储网格顶点对应的切线向量相关信息，
    // 在一些高级的图形渲染技术（比如法线贴图等）中，切线向量和法线向量等配合使用来实现更精确的光照、纹理映射等效果，但由于当前存在 bug，暂时无法使用该成员变量
    // UPROPERTY()
    // TArray<FProcMeshTangent> Tangents;
};
