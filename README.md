# 人车模拟器


该项目是一个用于研究人和车的开源仿真器。
基于 Carla 和 OpenSim 开发，旨在支持人车系统的开发、训练和验证。
除了开源代码外，还提供了为此目的创建的可自由使用的开放数字资产（城市布局、建筑、车辆、行人）。
该模拟平台支持传感器套件和环境条件的灵活规范。
具体介绍请参考 [中文文档](https://openhutb.github.io) 。

## 入门示例
1. 下载 [链接](https://pan.baidu.com/s/1n2fJvWff4pbtMe97GOqtvQ?pwd=hutb) 中的`software/car/DReyeVR`中的`hutb_*.zip`文件并解压；
2. 运行`WindowsNoEditor`文件夹下的`CarlaUE4.exe`，使用键盘进行控制；
3. 使用`pip install hutb`安装Python工具包，运行以下脚本在场景中生成车辆和行人：
```shell
python generate_traffic.py
```
显示手动控制车辆：
```shell
python manual_control.py
```



## 编译 Carla

使用`git clone`或从此页面下载项目。请注意，hutb分支包含最新版本以及最新的修复程序和功能。

然后按照 [如何在Windows上构建中文说明](https://github.com/OpenHUTB/roadrunner_scenario/blob/master/ug/carla_windows_build.mlx) 、[如何在Linux上构建][buildlinuxlink]中的说明进行操作。  

Linux版本需要一个UE补丁来解决有关Vulkan的一些可视化问题。
那些已经使用Linux构建的用户应该安装补丁，并使用以下命令重新构建UE。

[buildlinuxlink]: https://openhutb.github.io/doc/build_linux/
[buildwindowslink]: https://openhutb.github.io/doc/build_windows/


### 软硬件要求

* 处理器：Intel i7 gen 9th - 11th / Intel i9 gen 9th - 11th / AMD ryzen 7 / AMD ryzen 9
* 内存：+16 GB
* 显卡：NVIDIA RTX 2070 / NVIDIA RTX 2080 / NVIDIA RTX 3070, NVIDIA RTX 3080
* 操作系统：Windows 10, Ubuntu 18.04

## 生态系统

与模拟平台相关的存储库：

* [**CARLA 自动驾驶排行榜**](https://leaderboard.carla.org/): 用于验证自动驾驶技术栈的自动平台
* [**Scenario_Runner**](https://github.com/carla-simulator/scenario_runner): Carla 0.9.X中执行交通场景的引擎
* [**ROS-bridge**](https://github.com/carla-simulator/ros-bridge): Carla 0.9.X和ROS的接口
* [**驾驶基准**](https://github.com/carla-simulator/driving-benchmarks): 用于自动驾驶任务的基准工具
* [**条件模仿学习**](https://github.com/felipecode/coiltraine): Carla 中条件模拟学习（Conditional Imitation Learning）模型的训练和测试
* [**AutoWare AV stack**](https://github.com/carla-simulator/carla-autoware): 连接AutoWare AV 栈和 Carla 的桥接器
* [**强化学习**](https://github.com/carla-simulator/reinforcement-learning): Carla 中运行条件强化学习（Conditional Reinforcement Learning）模型的代码
* [**地图编辑器**](https://github.com/carla-simulator/carla-map-editor): 独立的GUI应用程序，可通过红绿灯和交通标志信息增强RoadRunner地图
* [**智能驾驶系统**](https://github.com/CAS-LRJ/ISS): 中科院自动驾驶算法开发、测试、仿真与验证平台 



## 其他

除了文档之外，还为用户创建了一些附加内容。这是一种涵盖不同主题的好方法，例如对特定模块的详细解释、功能的最新改进、未来的工作等等。

*   __常规__  
	*   艺术改进：环境和渲染 — [视频](https://youtu.be/ZZaHevsz8W8) | [PPT](https://drive.google.com/file/d/1l9Ztaq0Q8fNN5YPU4-5vL13eZUwsQl5P/view?usp=sharing)  
	*   核心实现：同步、快照和地标 — [视频](https://youtu.be/nyyTLmphqY4) | [PPT](https://drive.google.com/file/d/1yaOwf1419qWZqE1gTSrrknsWOhawEWh_/view?usp=sharing)
	*   数据摄入 — [视频](https://youtu.be/mHiUUZ4xC9o) | [PPT](https://drive.google.com/file/d/10uNBAMreKajYimIhwCqSYXjhfVs2bX31/view?usp=sharing)  
	*   行人及其实现 — [视频](https://youtu.be/Uoz2ihDwaWA) | [PPT](https://drive.google.com/file/d/1Tsosin7BLP1k558shtbzUdo2ZXVKy5CB/view?usp=sharing)  
	*   Carla 中的传感器 — [视频](https://youtu.be/T8qCSet8WK0) | [PPT](https://drive.google.com/file/d/1UO8ZAIOp-1xaBzcFMfn_IoipycVkUo4q/view?usp=sharing)  
*   __模块__  
	*   交通管理器的改进 — [视频](https://youtu.be/n9cufaJ17eA) | [PPT](https://drive.google.com/file/d/1R9uNZ6pYHSZoEBxs2vYK7swiriKbbuxo/view?usp=sharing)  
	*   汽车软件与ROS的集成 — [视频](https://youtu.be/ChIgcC2scwU) | [PPT](https://drive.google.com/file/d/1uO6nBaFirrllb08OeqGAMVLApQ6EbgAt/view?usp=sharing)  
	*   ScenarioRunner简介 — [视频](https://youtu.be/dcnnNJowqzM) | [PPT](https://drive.google.com/file/d/1zgoH_kLOfIw117FJGm2IVZZAIRw9U2Q0/view?usp=sharing)  
	*   OpenSCENARIO 支持 — [PPT](https://drive.google.com/file/d/1g6ATxZRTWEdstiZwfBN1_T_x_WwZs0zE/view?usp=sharing)  
*   __特点__  
	*   与SUMO和PTV Vissim的联合仿真 — [视频](https://youtu.be/PuFSbj1PU94) | [PPT](https://drive.google.com/file/d/10DgMNUBqKqWBrdiwBiAIT4DdR9ObCquI/view?usp=sharing)  
	*   RSS-lib 的集成 — [PPT](https://drive.google.com/file/d/1whREmrCv67fOMipgCk6kkiW4VPODig0A/view?usp=sharing)  
	*   外部传感器接口（External Sensor Interface，ESI） — [视频](https://youtu.be/5hXHPV9FIeY) | [PPT](https://drive.google.com/file/d/1VWFaEoS12siW6NtQDUkm44BVO7tveRbJ/view?usp=sharing)  
	*   OpenDRIVE 独立模式 — [视频](https://youtu.be/U25GhofVV1Q) | [PPT](https://drive.google.com/file/d/1D5VsgfX7dmgPWn7UtDDid3-OdS1HI4pY/view?usp=sharing)  

## 开发

windows操作系统下通过vs2019打开并编译carla：

1. 开Carla的CMake项目：

	File-->Open-->CMake, 在对话框中找到carla所在的本地文件夹（包含CMakeLists），选择CMakeLists.txt文件，打开，Visual studio会自动加载此仓库，解析CMakeLists.txt 文件，并提取其配置和变量信息。解析完成会从`解决方案资源管理器`中看到.cpp文件。

2. 修改配置
点击`x64-Debug`下拉菜单中的`管理配置`，并在弹出的界面点击`编辑JSON`，

将所需要构建的类型改为想编译的类型，比如`Client`。

3. 生成：
点击菜单栏`生成`-`全部生成`或`部分生成`即可。

## 发布

包含所有软件依赖，双击`launch.bat`启动软件。

