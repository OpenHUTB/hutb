<h1 align="center">OpenHUTB</h1>

<div align="center">
What I cannot create，I don't understand.

-- Richard Phillips Feynman, 1988
</div>

The OpenHUTB (Open source Human-vehicle Twin Build) is a photorealistic-quality physics simulator for studying human and vehicles ([humanoid](https://openhutb.github.io/doc/#_5), [autonomous vehicles](https://openhutb.github.io/doc/#_4), [drones](https://openhutb.github.io/air_doc/), and [submersibles](https://openhutb.github.io/mujoco_plugin/underwater/)), designed to support the development, training, and validation of robotic algorithms. In addition to open-source code, it provides freely usable open digital assets ([town layouts](https://openhutb.github.io/doc/core_map/#non-layered-maps), [buildings](https://openhutb.github.io/doc/catalogue/), [vehicles](https://openhutb.github.io/doc/catalogue_vehicles/), [pedestrians](https://openhutb.github.io/doc/catalogue_pedestrians/), [props](https://openhutb.github.io/doc/catalogue_props/), etc.) created for this purpose. The simulation platform supports standard specifications for [VR cockpit](https://openhutb.github.io/doc/interbehavior/), [unified air-ground](https://openhutb.github.io/air_doc/dev/Carla_Air/), [submersibles](https://openhutb.github.io/mujoco_plugin/underwater/), [sensors](https://openhutb.github.io/doc/ref_sensors/), [data synthesis](https://openhutb.github.io/doc/tuto_G_retrieve_data/), [traffic management](https://openhutb.github.io/doc/adv_traffic_manager/), [multiphysics simulation](https://openhutb.github.io/doc/tuto_G_chrono/), [pedestrian navigation](https://openhutb.github.io/doc/tuto_G_pedestrian_navigation/), and [Python interfaces](https://openhutb.github.io/doc/python_api/). For detailed information, please refer to the [documentation](https://openhutb.github.io).


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
  <b>English</b> | <a href="README.md">简体中文</a> &nbsp;&nbsp;|&nbsp;&nbsp;
  🌐 <a href="https://github.com/OpenHUTB/hutb"><b>Project Home</b></a> &nbsp;|&nbsp;
  📖 <a href="https://openhutb.github.io/hutb/"><b>Docs</b></a>
</p>


## Usage example
1. Download and run the [simulator download tool](https://gitee.com/OpenHUTB/sw/releases/download/up/hutb_downloader.exe);
2. Navigate to the generated directory `hutb/PythonAPI/carla/dist/`, and use `pip install hutb-*.whl` to install the toolkit for your specific Python version (supports Python 3.7-3.14). Run the following script to [generate vehicles and pedestrians](https://github.com/OpenHUTB/doc/blob/master/src/examples/generate_traffic.py)  in the scene: 
	```shell
	python PythonAPI/examples/generate_traffic.py
	```
	[Manual pedestrian control](https://github.com/OpenHUTB/doc/blob/master/src/examples/manual_control.py) ：
	```shell
	python PythonAPI/examples/manual_control.py --filter walker.pedestrian.*
	```
	Use [config.py](https://github.com/OpenHUTB/hutb/blob/hutb/PythonAPI/util/config.py) to [switch to](ue/switch_mode.md) [VR cockpit mode](https://openhutb.github.io/doc/interbehavior/) , and use the Logitech steering wheel or keyboard keys `W`, `A`, `S`, `D`, etc. for control; `Z` is reverse gear. 
	```shell
	python config.py --map Town10HD?GAME=VR
	```
	Switch to: [drone mode](https://openhutb.github.io/air_doc/) (Version 2.3.0 and later support both Carla mode and AirSim mode by default.)：
	```shell
	python config.py --map Town10HD?GAME=AIR
	# Press Enter after takeoff to enter different states.
	python PythonClient/multirotor/hello_drone.py
	```


## Source code compilation

Use `git clone` or download the project from this page.
```shell
# Launch the editor
setup.bat -l
# Package
setup.bat -p
```
Please note that the hutb branch contains the latest version, as well as the latest fixes and features. Alternatively, follow the instructions in [How to Build on Windows (Chinese Guide)](https://openhutb.github.io/doc/build_windows/) and [How to Build on Linux](https://openhutb.github.io/doc/build_linux/).


>[!NOTE]
> Artists can skip compiling and directly download the `software/hutb/hutb_editor.zip` file from the [link](https://pan.baidu.com/s/1n2fJvWff4pbtMe97GOqtvQ?pwd=hutb), extract it, and double-click `launch_hutb_editor.bat` to launch the Unreal Editor with the plugin. 


### Hardware and software requirements

* Processor: Intel i7 gen 9th - 11th / Intel i9 gen 9th - 11th / AMD ryzen 7 / AMD ryzen 9
* Memory: +16 GB
* Graphics card: NVIDIA RTX 2070 or higher
* OS: Windows 10+、Ubuntu 18.04+、MacOS 12+。

## Ecosystem

Repositories related to the simulation platform:

* [**Related applications**](https://openhutb.github.io/doc/used_by/) include: [perception](https://openhutb.github.io/doc/used_by/#perception) 、[planning](https://openhutb.github.io/doc/used_by/#planning) 、[control](https://openhutb.github.io/doc/used_by/#control) 、[end-to-end](https://openhutb.github.io/doc/used_by/#end_2_end) 、[large language models](https://openhutb.github.io/doc/used_by/#llm) 、[pedestrians](https://openhutb.github.io/doc/used_by/#pedestrian) 、[agents](https://openhutb.github.io/doc/used_by/#agent) 、[explainability](https://openhutb.github.io/doc/used_by/#explainability) ,etc.
* [**Autonomous Driving Leaderboard**](https://leaderboard.carla.org/): An autonomous platform used to validate autonomous driving technology stacks
* [**Nvidia ecosystem**](https://openhutb.github.io/doc/nvidia/): [SimReady](https://openhutb.github.io/doc/nvidia_simready/) 、[Neural rendering](https://openhutb.github.io/doc/nvidia_nurec/) 、[Cosmos Cosmos Transfer](https://openhutb.github.io/doc/nvidia_cosmos_transfer/), etc.
* [**Scenario Runner**](https://github.com/carla-simulator/scenario_runner): The engine that executes traffic scenarios in the simulator
* [**ROS-bridge**](https://github.com/carla-simulator/ros-bridge): Interface between simulator and ROS
* [**Driving Benchmark**](https://github.com/carla-simulator/driving-benchmarks): Benchmarking tools for autonomous driving tasks
* [**AutoWare AV stack**](https://github.com/carla-simulator/carla-autoware): Bridge connecting the AutoWare AV stack and Carla
* [**Apollo Bridge**](https://github.com/guardstrikelab/carla_apollo_bridge): A data and control bridge for the communication between the latest version of Apollo and Carla
* [**Map Editor**](https://github.com/carla-simulator/carla-map-editor): A standalone GUI application that enhances RoadRunner maps with traffic light and sign information
* [**Reinforcement Learning**](https://openhutb.github.io/doc/used_by/#rl): Code for various reinforcement learning models


## Others

In addition to the documentation, supplementary content has been created for users. This is a good way to cover different topics, such as detailed explanations of specific modules, the latest improvements to features, future work, and so on.

*   __Conventional__  
	*   Artistic Improvements: Environment and Rendering — [Video](https://youtu.be/ZZaHevsz8W8) | [PPT](https://drive.google.com/file/d/1l9Ztaq0Q8fNN5YPU4-5vL13eZUwsQl5P/view?usp=sharing)  
	*   Core implementation: synchronization, snapshots, and landmarks — [Video](https://youtu.be/nyyTLmphqY4) | [PPT](https://drive.google.com/file/d/1yaOwf1419qWZqE1gTSrrknsWOhawEWh_/view?usp=sharing)
	*   Data Ingestion — [Video](https://youtu.be/mHiUUZ4xC9o) | [PPT](https://drive.google.com/file/d/10uNBAMreKajYimIhwCqSYXjhfVs2bX31/view?usp=sharing)  
	*   Pedestrians and their realization — [Video](https://youtu.be/Uoz2ihDwaWA) | [PPT](https://drive.google.com/file/d/1Tsosin7BLP1k558shtbzUdo2ZXVKy5CB/view?usp=sharing)  
	*   Sensors in the simulator — [Video](https://youtu.be/T8qCSet8WK0) | [PPT](https://drive.google.com/file/d/1UO8ZAIOp-1xaBzcFMfn_IoipycVkUo4q/view?usp=sharing)  
*   __Modules__  
	*   Mujoco plugin for simulator — [mujoco_plugin](https://github.com/OpenHUTB/mujoco_plugin)
	*   Drone plugin for simulator — [air](https://github.com/OpenHUTB/air)
	*   VR Driving — [interbehavior](https://openhutb.github.io/doc/interbehavior/)  
	*   Traffic Manager Improvements — [Video](https://youtu.be/n9cufaJ17eA) | [PPT](https://drive.google.com/file/d/1R9uNZ6pYHSZoEBxs2vYK7swiriKbbuxo/view?usp=sharing)
	*   Integration of automotive software with ROS — [Video](https://youtu.be/ChIgcC2scwU) | [PPT](https://drive.google.com/file/d/1uO6nBaFirrllb08OeqGAMVLApQ6EbgAt/view?usp=sharing)  
	*   ScenarioRunner Introduction — [Video](https://youtu.be/dcnnNJowqzM) | [PPT](https://drive.google.com/file/d/1zgoH_kLOfIw117FJGm2IVZZAIRw9U2Q0/view?usp=sharing)  
	*   OpenSCENARIO Support — [PPT](https://drive.google.com/file/d/1g6ATxZRTWEdstiZwfBN1_T_x_WwZs0zE/view?usp=sharing)  
	*   Biomechanics of human movement — [Video](https://github.com/OpenHUTB/move) | [PPT](https://drive.google.com/file/d/1g6ATxZRTWEdstiZwfBN1_T_x_WwZs0zE/view?usp=sharing) | [Video](https://opensimconfluence.atlassian.net/wiki/spaces/OpenSim/pages/53088695/Examples+and+Tutorials)
*   __Features__  
	*   Co-simulation with SUMO and PTV Vissim — [Video](https://youtu.be/PuFSbj1PU94) | [PPT](https://drive.google.com/file/d/10DgMNUBqKqWBrdiwBiAIT4DdR9ObCquI/view?usp=sharing)  
	*   Integration of RSS-lib — [PPT](https://drive.google.com/file/d/1whREmrCv67fOMipgCk6kkiW4VPODig0A/view?usp=sharing)  
	*   External sensor interface（External Sensor Interface，ESI） — [Video](https://youtu.be/5hXHPV9FIeY) | [PPT](https://drive.google.com/file/d/1VWFaEoS12siW6NtQDUkm44BVO7tveRbJ/view?usp=sharing)  
	*   OpenDRIVE Standalone Mode — [Video](https://youtu.be/U25GhofVV1Q) | [PPT](https://drive.google.com/file/d/1D5VsgfX7dmgPWn7UtDDid3-OdS1HI4pY/view?usp=sharing)  


## References and Licenses

This project stands on the shoulders of giants, and we sincerely thank the developers of the following open-source projects: [Carla](https://github.com/carla-simulator/carla) (MIT License), [AirSim](https://github.com/microsoft/AirSim) (MIT License), [DReyeVR](https://openhutb.github.io/doc/interbehavior/) (MIT License), [CarlaAir](https://github.com/louiszengCN/CarlaAir) (MIT License), (MIT License), and [OpenSim](https://openhutb.github.io/doc/pedestrian/tuto_content_chrono_opensim/) (Apache License).

Related assets are licensed under the CC-BY license. Other related assets (including map scenes of [Hunan University of Technology and Business](https://www.hutb.edu.cn/),  [Changsha CEC Software Park](https://map.baidu.com/poi/%E9%95%BF%E6%B2%99%E4%B8%AD%E7%94%B5%E8%BD%AF%E4%BB%B6%E5%9B%AD/@12566933.66,3258249.376047685,17.83z?uid=694ec5236c53273882a00c5a&ugc_type=3&ugc_ver=1&device_ratio=1&compat=1&en_uid=694ec5236c53273882a00c5a&pcevaname=pc4.1&querytype=detailConInfo&da_src=shareurl), [Chinese Vehicles](https://openhutb.github.io/doc/catalogue_vehicles/), etc.) and code are open source under the MIT license.


