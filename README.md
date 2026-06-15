# 人车模拟器

该项目是一个用于研究[具身人](https://openhutb.github.io/doc/#_5)、[无人车](https://openhutb.github.io/doc/#_4)、[无人机](https://openhutb.github.io/air_doc/)的影视级物理模拟器，旨在支持人和载具算法的开发、训练和验证。
除了开源代码外，还提供了为此目的创建的可自由使用的开放数字资产（[城镇布局](https://openhutb.github.io/doc/core_map/#non-layered-maps) 、[建筑](https://openhutb.github.io/doc/catalogue/) 、[载具](https://openhutb.github.io/doc/catalogue_vehicles/) 、[行人](https://openhutb.github.io/doc/catalogue_pedestrians/) 、[道具](https://openhutb.github.io/doc/catalogue_props/) 等）。
该模拟平台支持 [传感器](https://openhutb.github.io/doc/ref_sensors/) 、[数据合成](https://openhutb.github.io/doc/tuto_G_retrieve_data/) 、[交通管理器](https://openhutb.github.io/doc/adv_traffic_manager/) 、[多物理场仿真](https://openhutb.github.io/doc/tuto_G_chrono/) 、[行人导航](https://openhutb.github.io/doc/tuto_G_pedestrian_navigation/) 、[Python接口](https://openhutb.github.io/doc/python_api/) 等的标准规范。
详细介绍请参考 [文档](https://openhutb.github.io) 。

<!-- https://github.com/inttter/md-badges -->
<div align="center">
  <img src="https://img.shields.io/badge/GitHub_Actions-2088FF?logo=github-actions&logoColor=white" alt="CICD: Github Action"/>
  <img src="https://img.shields.io/badge/Unreal%20Engine-%23313131.svg?logo=unrealengine&logoColor=white" alt="CICD: Github Action"/>
  <img src="https://img.shields.io/badge/MkDocs-526CFE?logo=materialformkdocs&logoColor=fff" alt="CICD: Github Action"/>
  <img src="https://img.shields.io/badge/GitHub%20Pages-121013?logo=github&logoColor=white" alt="CICD: Github Action"/>
  <img src="https://img.shields.io/badge/Git-F05032?logo=git&logoColor=fff" alt="Python 3.7+"/>
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="License: Non-Commercial"/>
  <img src="https://img.shields.io/badge/PyTorch-ee4c2c?logo=pytorch&logoColor=white" alt="Python 3.7+"/>
  <img src="https://img.shields.io/badge/C++-%2300599C.svg?logo=c%2B%2B&logoColor=white" alt="Python 3.7+"/>
  <img src="https://img.shields.io/badge/Python-3776AB?logo=python&logoColor=fff" alt="Python 3.7+"/>
  <img src="https://img.shields.io/badge/Ubuntu-E95420?logo=ubuntu&logoColor=white" alt="Platform"/>
  <img src="https://custom-icon-badges.demolab.com/badge/Windows-0078D6?logo=windows11&logoColor=white" alt="Platform"/>
</div>

<p align="center">
  <a href="README_EN.md">English</a> | <b>简体中文</b> &nbsp;&nbsp;|&nbsp;&nbsp;
  🌐 <a href="https://github.com/OpenHUTB/hutb"><b>项目主页</b></a> &nbsp;|&nbsp;
  📖 <a href="https://openhutb.github.io/"><b>中文文档</b></a>
</p>


## 使用示例
1. 下载并执行 [模拟器下载工具](https://gitee.com/OpenHUTB/sw/releases/download/up/hutb_downloader.exe) ；
2. 进入生成的目录`hutb/PythonAPI/carla/dist/`，使用`pip install hutb-*.whl`安装特定 Python 版本的工具包（支持Python 3.7-3.14），运行以下脚本在场景中 [生成车辆和行人](https://github.com/OpenHUTB/doc/blob/master/src/examples/generate_traffic.py) ：
	```shell
	python PythonAPI/examples/generate_traffic.py
	```
	[手动控制行人](https://github.com/OpenHUTB/doc/blob/master/src/examples/manual_control.py) ：
	```shell
	python PythonAPI/examples/manual_control.py --filter walker.pedestrian.*
	```
	使用 [config.py](https://github.com/OpenHUTB/hutb/blob/hutb/PythonAPI/util/config.py) [切换](ue/switch_mode.md) 到 [VR 驾驶舱模式](https://openhutb.github.io/doc/interbehavior/) ，使用罗技方向盘或键盘`W`、`A`、`S`、`D`等进行控制，`Z`为倒档：
	```shell
	python config.py --map Town10HD?GAME=VR
	```
	切换到 [无人机模式](https://openhutb.github.io/air_doc/) ：
	```shell
	python config.py --map Town10HD?GAME=AIR
	# 起飞后按回车键进入不同状态
	python PythonClient/multirotor/hello_drone.py
	```


## 源码编译

使用`git clone`或从此页面下载项目。
```shell
# 启动编辑器
setup.bat -l
# 打包
setup.bat -p
```
请注意，hutb分支包含最新版本以及最新的修复程序和功能。
或者按照 [如何在Windows上构建中文说明](https://openhutb.github.io/doc/build_windows/) 、[如何在Linux上构建](https://openhutb.github.io/doc/build_linux/) 中的说明进行操作。


>[!NOTE]
> 艺术创作人员可以不编译，直接下载 [链接](https://pan.baidu.com/s/1n2fJvWff4pbtMe97GOqtvQ?pwd=hutb) 中的`software/hutb/hutb_editor.zip`文件并解压，双击`launch_hutb_editor.bat`即可启动带插件的虚幻编辑器。


### 软硬件要求

* 处理器：Intel i7 gen 9th - 11th / Intel i9 gen 9th - 11th / AMD ryzen 7 / AMD ryzen 9
* 内存：+16 GB
* 显卡：NVIDIA RTX 2070 以上
* 操作系统：Windows 10+、Ubuntu 18.04+、MacOS 12+。

## 生态系统

与模拟平台相关的存储库：

* [**相关应用**](https://openhutb.github.io/doc/used_by/): 包括 [感知](https://openhutb.github.io/doc/used_by/#perception) 、[规划](https://openhutb.github.io/doc/used_by/#planning) 、[控制](https://openhutb.github.io/doc/used_by/#control) 、[端到端](https://openhutb.github.io/doc/used_by/#end_2_end) 、[大模型](https://openhutb.github.io/doc/used_by/#llm) 、[行人](https://openhutb.github.io/doc/used_by/#pedestrian) 、[智能体](https://openhutb.github.io/doc/used_by/#agent) 、[可解释](https://openhutb.github.io/doc/used_by/#explainability) 等
* [**自动驾驶排行榜**](https://leaderboard.carla.org/): 用于验证自动驾驶技术栈的自动平台
* [**Nvidia 生态**](https://openhutb.github.io/doc/nvidia/): [SimReady](https://openhutb.github.io/doc/nvidia_simready/) 、[神经渲染](https://openhutb.github.io/doc/nvidia_nurec/) 、[Cosmos 世界基础模型](https://openhutb.github.io/doc/nvidia_cosmos_transfer/) 等
* [**Scenario Runner**](https://github.com/carla-simulator/scenario_runner): 模拟器中执行交通场景的引擎
* [**ROS-bridge**](https://github.com/carla-simulator/ros-bridge): 模拟器和ROS的接口
* [**驾驶基准**](https://github.com/carla-simulator/driving-benchmarks): 用于自动驾驶任务的基准工具
* [**AutoWare AV stack**](https://github.com/carla-simulator/carla-autoware): 连接AutoWare AV 栈和模拟器的桥接器
* [**Apollo 和模拟器的通信桥**](https://github.com/guardstrikelab/carla_apollo_bridge)：为 Apollo 和模拟器之间的通信提供数据和控制桥
* [**地图编辑器**](https://github.com/carla-simulator/carla-map-editor): 独立的GUI应用程序，可通过红绿灯和交通标志信息增强RoadRunner地图
* [**强化学习**](https://openhutb.github.io/doc/used_by/#rl): 各种强化学习模型的代码


## 其他

除了文档之外，还为用户创建了一些附加内容。这是一种涵盖不同主题的好方法，例如对特定模块的详细解释、功能的最新改进、未来的工作等。

*   __常规__  
	*   艺术改进：环境和渲染 — [视频](https://youtu.be/ZZaHevsz8W8) | [PPT](https://drive.google.com/file/d/1l9Ztaq0Q8fNN5YPU4-5vL13eZUwsQl5P/view?usp=sharing)  
	*   核心实现：同步、快照和地标 — [视频](https://youtu.be/nyyTLmphqY4) | [PPT](https://drive.google.com/file/d/1yaOwf1419qWZqE1gTSrrknsWOhawEWh_/view?usp=sharing)
	*   数据摄入 — [视频](https://youtu.be/mHiUUZ4xC9o) | [PPT](https://drive.google.com/file/d/10uNBAMreKajYimIhwCqSYXjhfVs2bX31/view?usp=sharing)  
	*   行人及其实现 — [视频](https://youtu.be/Uoz2ihDwaWA) | [PPT](https://drive.google.com/file/d/1Tsosin7BLP1k558shtbzUdo2ZXVKy5CB/view?usp=sharing)  
	*   模拟器中的传感器 — [视频](https://youtu.be/T8qCSet8WK0) | [PPT](https://drive.google.com/file/d/1UO8ZAIOp-1xaBzcFMfn_IoipycVkUo4q/view?usp=sharing)  
*   __模块__  
    *   模拟器的 Mujoco 插件 — [mujoco_plugin](https://github.com/OpenHUTB/mujoco_plugin)
    *   模拟器的无人机插件 — [air](https://github.com/OpenHUTB/air)
    *   VR 驾驶 — [interbehavior](https://openhutb.github.io/doc/interbehavior/)  
	*   交通管理器的改进 — [视频](https://youtu.be/n9cufaJ17eA) | [PPT](https://drive.google.com/file/d/1R9uNZ6pYHSZoEBxs2vYK7swiriKbbuxo/view?usp=sharing)
	*   汽车软件与ROS的集成 — [视频](https://youtu.be/ChIgcC2scwU) | [PPT](https://drive.google.com/file/d/1uO6nBaFirrllb08OeqGAMVLApQ6EbgAt/view?usp=sharing)  
	*   ScenarioRunner简介 — [视频](https://youtu.be/dcnnNJowqzM) | [PPT](https://drive.google.com/file/d/1zgoH_kLOfIw117FJGm2IVZZAIRw9U2Q0/view?usp=sharing)  
	*   OpenSCENARIO 支持 — [PPT](https://drive.google.com/file/d/1g6ATxZRTWEdstiZwfBN1_T_x_WwZs0zE/view?usp=sharing)  
	*   人运动的生物力学 — [教程](https://github.com/OpenHUTB/move) | [PPT](https://drive.google.com/file/d/1g6ATxZRTWEdstiZwfBN1_T_x_WwZs0zE/view?usp=sharing) | [示例](https://opensimconfluence.atlassian.net/wiki/spaces/OpenSim/pages/53088695/Examples+and+Tutorials)
*   __特点__  
	*   与SUMO和PTV Vissim的联合仿真 — [视频](https://youtu.be/PuFSbj1PU94) | [PPT](https://drive.google.com/file/d/10DgMNUBqKqWBrdiwBiAIT4DdR9ObCquI/view?usp=sharing)  
	*   RSS-lib 的集成 — [PPT](https://drive.google.com/file/d/1whREmrCv67fOMipgCk6kkiW4VPODig0A/view?usp=sharing)  
	*   外部传感器接口（External Sensor Interface，ESI） — [视频](https://youtu.be/5hXHPV9FIeY) | [PPT](https://drive.google.com/file/d/1VWFaEoS12siW6NtQDUkm44BVO7tveRbJ/view?usp=sharing)  
	*   OpenDRIVE 独立模式 — [视频](https://youtu.be/U25GhofVV1Q) | [PPT](https://drive.google.com/file/d/1D5VsgfX7dmgPWn7UtDDid3-OdS1HI4pY/view?usp=sharing)  


## 参考和许可证

```
@inproceedings{Dosovitskiy17,
  title = {{CARLA}: {An} Open Urban Driving Simulator},
  author = {Alexey Dosovitskiy and German Ros and Felipe Codevilla and Antonio Lopez and Vladlen Koltun},
  booktitle = {Proceedings of the 1st Annual Conference on Robot Learning},
  pages = {1--16},
  year = {2017}
}
```


```
@article{jdan,
	author={Haidong Wang and Xuan He and Zhiyong Li and Jin Yuan and Shutao Li},
	title={JDAN: Joint Detection and Association Network for Real-Time Online Multi-Object Tracking.},
	journal={ACM Transactions on Multimedia Computing, Communications, and Applications},
	volume=19,
	year=2023,
}
```

该项目站在巨人的肩膀上，诚挚感谢以下开源项目的开发者：
[Carla](https://github.com/carla-simulator/carla) (MIT 许可证)、[AirSim](https://github.com/microsoft/AirSim) (MIT 许可证)、[DReyeVR](https://openhutb.github.io/doc/interbehavior/) (MIT 许可证)、[CarlaAir](https://github.com/louiszengCN/CarlaAir)(MIT 许可证)、[OpenSim](https://openhutb.github.io/doc/pedestrian/tuto_content_chrono_opensim/) (Apache 许可证)。

Carla 相关的资产遵循 CC-BY 许可证。
其他相关资产（包括[湖南工商大学大学](https://www.hutb.edu.cn/)、[长沙中电软件园](https://map.baidu.com/poi/%E9%95%BF%E6%B2%99%E4%B8%AD%E7%94%B5%E8%BD%AF%E4%BB%B6%E5%9B%AD/@12566933.66,3258249.376047685,17.83z?uid=694ec5236c53273882a00c5a&ugc_type=3&ugc_ver=1&device_ratio=1&compat=1&en_uid=694ec5236c53273882a00c5a&pcevaname=pc4.1&querytype=detailConInfo&da_src=shareurl)等地图场景）和代码基于 [MIT 许可证](./LICENSE) 开源。
