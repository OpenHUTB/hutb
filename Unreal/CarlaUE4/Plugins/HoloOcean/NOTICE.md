# 来源与许可说明

本插件（目录名 `HoloOcean`，模块名 `Holodeck`）基于开源项目 **HoloOcean / Holodeck**（BYU PCCL）修改而来，
以 **MIT License** 授权，全文见同目录 [`LICENSE`](LICENSE)。

## 原始版权

```
Copyright (c) 2019 BYU PCCL
```

`Source/Holodeck/Holodeck.Build.cs` 首行保留原始声明：

```cpp
// MIT License (c) 2019 BYU PCCL see LICENSE file
```

## 本副本的改动

OpenHUTB 在引入时对插件做了以下改动，均基于 MIT 许可允许的范围：

- `HoloOcean.uplugin` 中的 `CreatedBy` 字段被改为 `OpenHUTB`（原为 BYU PCCL）
- `DocsURL` / `SupportURL` 指向 OpenHUTB 的仓库与文档站
- 模块类型由 `Editor` 调整为 `Runtime`，以便在打包构建中使用（2026-11-04）
- `Source/Holodeck/Holodeck.cpp`：`IMPLEMENT_PRIMARY_GAME_MODULE` 改为 `IMPLEMENT_MODULE`
  （修复打包时 LNK2005 重复符号，详见下节）

### 关于 IMPLEMENT_MODULE 的修改

`Holodeck.cpp` 原使用：

```cpp
IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Holodeck, "Holodeck" );
```

该宏专供工程的「主游戏模块」使用，会定义 `GInternalProjectName`、`GLiveCodingProject`、
`GObjectArrayForDebugVisualizers`、`operator new/delete` 等一整套全局符号。

编辑器构建时模块编成独立 DLL，不冲突；但模块类型改为 `Runtime` 后，
游戏目标会把插件模块链接进单体可执行文件，与工程主模块 `CarlaUE4.cpp` 重复定义：

```
error LNK2005: "operator new" already defined in Module.Holodeck.cpp.obj
error LNK2005: "wchar_t * GInternalProjectName" already defined in Module.Holodeck.cpp.obj
fatal error LNK1169: one or more multiply defined symbols found
```

改为 `IMPLEMENT_MODULE( FDefaultGameModuleImpl, Holodeck );` 后打包正常。

**这是上游 HoloOcean 的缺陷**——插件本就不应使用 `IMPLEMENT_PRIMARY_GAME_MODULE`。

## MIT 合规要求

MIT 许可要求：**在软件的所有副本或实质性部分中，保留上述版权声明与本许可声明。**

因此本目录必须始终保留 `LICENSE` 文件。打包分发（exe / 安装器）时，
需将本文件一并纳入安装包的许可声明目录。

## 上游项目

- HoloOcean 官方文档：https://holoocean.readthedocs.io/
- 维护方：Brigham Young University, Perception, Control and Cognition Lab (BYU PCCL)
