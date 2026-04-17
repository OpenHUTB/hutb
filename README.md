# CARLA 高保真物理仿真实验平台 (Windows 部署版)
[![Python 3.10](https://img.shields.io/badge/python-3.10.11-blue.svg)](https://www.python.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## 📖 项目概述
本项目基于 CARLA (Car Learning to Act) 开源引擎，旨在构建一个用于自动驾驶算法验证与具身智能研究的高保真模拟环境。通过集成 OpenHUTB 的 API 扩展包，实现了高性能的客户端-服务器（C/S）架构通信，支持复杂的交通流仿真与动态气象模拟。

## 🛠 实验环境规范 (Environment Matrix)
为确保仿真结果的可重复性，本项目在以下环境下完成部署验证：

| 组件 | 规格/版本 | 备注 |
| :--- | :--- | :--- |
| **操作系统** | Windows 10/11 x64 | 宿主环境 |
| **仿真引擎** | CARLA UE4 Engine | 核心物理后端 |
| **Python Runtime** | 3.10.11 | 执行环境 |
| **API 协议** | hutb-2.9.16 (cp310) | 通讯中间件 |
| **关键依赖** | NumPy, OpenCV, Msgpack | 数据处理与视觉库 |

## 🚀 部署与执行流程

### I. 模拟器服务端初始化 (Simulator Server)
在仿真实验开始前，需启动基于虚幻引擎 4 (UE4) 的服务端程序，以初始化物理世界与渲染管线：
```powershell
# 定位至根目录执行
.\CarlaUE4.exe -windowed -carla-server
II. 自动化交通流注入 (Traffic Injection)
利用异步通信机制，在场景中实例化动态障碍物。通过调整参数 -n 可改变环境复杂度：

PowerShell

# 建议配置：80 辆载具与 20 名行人
python PythonAPI/examples/generate_traffic.py -n 80 -w 20
III. 动态气象条件配置 (Environmental Control)
实验支持对日照强度、降水量、路面湿度等气象因子进行实时控制：

PowerShell

# 启动气象循环动力学脚本
python PythonAPI/examples/dynamic_weather.py
IV. 手动介入与逻辑验证 (Manual Interaction)
支持通过键盘外设接管 ego-vehicle，用于验证碰撞检测逻辑与车辆动力学模型：

PowerShell

python PythonAPI/examples/manual_control.py
📊 核心特性声明
高保真渲染：支持基于光线追踪原理的实时视觉输出。

物理精确性：基于 OpenDrive 标准的道路拓扑结构验证。

可扩展性：支持通过 Python API 自定义传感器数据采集流（Lidar, RGB, IMU）。

Maintainer: Jiang Meng (蒋萌)

Affiliation: OpenHUTB Open Source Community / Academic Project

Last Updated: 2026-04
feat: 升级为专业版实验文档及本地环境审计
