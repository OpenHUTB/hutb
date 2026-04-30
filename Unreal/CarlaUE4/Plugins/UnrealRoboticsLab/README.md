[![Documentation](https://img.shields.io/badge/Documentation-blue.svg)](https://urlab-sim.github.io/UnrealRoboticsLab/)
[![arXiv](https://img.shields.io/badge/arXiv-2504.14135-b31b1b.svg)](https://arxiv.org/abs/2504.14135)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![MuJoCo](https://img.shields.io/badge/MuJoCo-3.7+-green.svg)](https://github.com/google-deepmind/mujoco)
[![Unreal Engine](https://img.shields.io/badge/Unreal_Engine-5.7+-black.svg)](https://www.unrealengine.com)

# URLab -- MuJoCo Physics in Unreal Engine

![URLab](docs/images/hero.png)

URLab (Unreal Robotics Lab) is an Unreal Engine 5 plugin that embeds the [MuJoCo](https://github.com/google-deepmind/mujoco) physics engine directly into the editor and runtime. Drag-and-drop MJCF XML import, a component-based architecture that maps 1:1 to MuJoCo elements, the full MuJoCo C API accessible from C++ and Blueprints, ZMQ networking for external control, Python policy integration, 40+ sensor types, 8 actuator types, debug visualization, and a record/replay system.

## When To Use URLab

- You need photorealistic rendering with accurate contact physics (sim-to-real transfer, synthetic data generation).
- You want to control MuJoCo robots from Python or ROS 2 while rendering in Unreal Engine.
- You want Unreal's ecosystem (Blueprints, Sequencer, Marketplace assets, Niagara) with physics-accurate robots.

## Quick Start

1. Clone into your project's `Plugins/` folder. Build third-party libraries once (`third_party/build_all.ps1`).
2. Drag an MJCF `.xml` file into the Content Browser. The importer creates a complete Articulation Blueprint.
3. Place an `AAMjManager` in your level. Place the generated Blueprint.
4. Hit Play. MuJoCo runs on an async physics thread; Unreal handles rendering.

See the [Getting Started](https://urlab-sim.github.io/UnrealRoboticsLab/getting_started/) guide for a full walkthrough.

## Key Features

- **MJCF Import** -- drag-and-drop `.xml`, builds full body/joint/actuator/sensor tree as an Articulation Blueprint.
- **Quick Convert** -- attach `UMjQuickConvertComponent` to any Static Mesh and it becomes a MuJoCo physics body. No XML needed.
- **40+ sensors, 8 actuator types, 4 joint types** -- position, velocity, motor, muscle, damper, adhesion, cylinder, general actuators; hinge, slide, ball, free joints; accelerometer, gyro, force/torque, touch, rangefinder, frame tracking, camera, and more.
- **ZMQ networking** -- stream sensor data out and receive actuator commands in via PUB/SUB sockets. Connect Python, ROS 2, or any ZMQ-speaking process.
- **Debug visualization** -- toggle contact forces, joint axes, collision geometry (articulations and Quick Convert separately), all via hotkeys.
- **Record and replay** -- capture trajectories, play back frame-by-frame, CSV import, snapshot/restore full simulation state.
- **Blueprint API** -- compile, reset, step, read sensors, set controls, capture snapshots, toggle debug drawing, all from Blueprint.
- **MjSimulate dashboard** -- in-editor widget with physics parameter tuning, actuator sliders, sensor readouts, live camera feeds.
- **Keyframe system** -- reset to named keyframes, hold keyframe poses (via ctrl or direct qpos injection), per-articulation dropdown in the dashboard.
- **Possess and walk** -- possess any articulation as a Pawn, WASD control with twist velocity commands over ZMQ.
- **Cinematic tools** -- orbit camera, keyframe camera with waypoint paths, impulse launchers for perturbation testing.
- **CoACD decomposition** -- one-click convex decomposition for non-convex meshes, cached by content hash.
- **Heightfield terrain** -- convert Unreal Landscape actors into MuJoCo heightfields.

See the [full documentation](https://urlab-sim.github.io/UnrealRoboticsLab/) for details.

## Python Integration

URLab communicates with external systems over ZMQ. The companion package [**urlab_bridge**](https://urlab-sim.github.io/UnrealRoboticsLab/guides/policy_bridge/) (separate repository, same organization) provides Python middleware for remote control, RL policy deployment, sensor monitoring, and ROS 2 bridging.

## Requirements

- **Unreal Engine 5.7+**
- **Windows** (Win64). Linux is experimental.
- **MuJoCo 3.7+** -- bundled in `third_party/`, built from source.
- **Visual Studio 2022** or compatible C++ toolchain.
- **CMake 3.24+** -- for building third-party libraries.
- **Python 3.11+** -- optional, for `urlab_bridge` policies.
- **[uv](https://github.com/astral-sh/uv)** -- optional, for Python dependency management.

## Installation

> **⚠️ Critical:** This is a C++ plugin. You **must** be using a C++ project. If your project is Blueprints-only, add a dummy C++ class via *Tools > New C++ Class* before starting.

### 1. Clone the Plugin
Clone this repo into your project's `Plugins` folder:
```bash
cd "YourProject/Plugins"
git clone https://github.com/URLab-Sim/UnrealRoboticsLab.git
```

### 2. Build Dependencies
Navigate to the plugin's `third_party` folder and run the build script to fetch and compile MuJoCo, CoACD, and ZMQ:
```powershell
cd UnrealRoboticsLab/third_party
.\build_all.ps1
```
*(If this script fails with a **Stack Overflow** error, see [Troubleshooting](#troubleshooting) below).*

### 3. Compile & Launch
1. Right-click your `.uproject` and select **Generate Visual Studio project files**.
2. Build the solution in VS2022/Rider and launch the Editor.
3. **Important:** In the Content Browser, go to **Settings (Gear Icon)** and check **"Show Plugin Content"** to see the UI and assets.

---

## Troubleshooting

### "CL.exe" Stack Overflow (Error 0xC00000FD)
If `build_all.ps1` fails during the MuJoCo sensor build, your compiler has run out of internal memory. This is an upstream issue with older MSVC toolsets.
* **Fix:** Update Visual Studio 2022 to the latest (17.10+) or use **VS 2025**.
* **Workaround:** Run the build with an increased stack flag:
    `cmake -B build ... -DCMAKE_CXX_FLAGS="/F10000000"`

### "Simulate" Dashboard is Missing
* Ensure an `MjManager` actor is placed in your level.
* Check that "Show Plugin Content" is enabled in your Content Browser settings.

---

## Architecture

```
AAMjManager (singleton coordinator, one per level)
  |-- UMjPhysicsEngine      (async physics loop, mjModel/mjData lifecycle)
  |-- UMjDebugVisualizer    (contact forces, collision wireframes, joint axes)
  |-- UMjNetworkManager     (ZMQ discovery, camera streaming)
  |-- UMjInputHandler       (hotkey processing)
  |
  |-- AMjArticulation (robot / mechanism, possessable Pawn)
  |     |-- UMjBody -> UMjHingeJoint, UMjGeom, UMjSensor, ...
  |     |-- UMjActuator (position, velocity, motor, muscle, ...)
  |     '-- UMjKeyframe, UMjDefault, UMjEquality, UMjTendon
  |
  |-- UMjQuickConvertComponent (on any Static Mesh actor)
  |-- AMjHeightfieldActor (Landscape -> MuJoCo heightfield)
  '-- WBP_MjSimulate (dashboard widget)
```

## Documentation

| Guide | Description |
|-------|-------------|
| [Getting Started](https://urlab-sim.github.io/UnrealRoboticsLab/getting_started/) | Installation and first simulation |
| [Features](https://urlab-sim.github.io/UnrealRoboticsLab/features/) | Complete feature reference |
| [MJCF Import](https://urlab-sim.github.io/UnrealRoboticsLab/guides/mujoco_import/) | Importing MuJoCo XML models |
| [Geometry & Collision](https://urlab-sim.github.io/UnrealRoboticsLab/guides/geometry_authoring/) | Collision shapes, meshes, Quick Convert |
| [Controller Framework](https://urlab-sim.github.io/UnrealRoboticsLab/guides/controller_framework/) | PD, keyframe, and custom controllers |
| [Blueprint Reference](https://urlab-sim.github.io/UnrealRoboticsLab/guides/blueprint_reference/) | Hotkeys, scripting, API usage |
| [ZMQ Networking](https://urlab-sim.github.io/UnrealRoboticsLab/guides/zmq_networking/) | External control via Python, ROS 2 |
| [Possession & Twist](https://urlab-sim.github.io/UnrealRoboticsLab/guides/possession_twist/) | WASD control, possess robots, twist commands |
| [URLab Bridge](https://urlab-sim.github.io/UnrealRoboticsLab/guides/policy_bridge/) | Python middleware, policies, remote control |
| [Architecture](https://urlab-sim.github.io/UnrealRoboticsLab/architecture/) | Full technical internals reference |

## Third-Party Licenses

| Library | License | Usage |
|---------|---------|-------|
| [MuJoCo](https://github.com/google-deepmind/mujoco) | Apache 2.0 | Physics engine |
| [CoACD](https://github.com/SarahWeiii/CoACD) | MIT | Convex mesh decomposition |
| [libzmq](https://zeromq.org) | MPL 2.0 | Network transport |

See [ThirdPartyNotices.txt](ThirdPartyNotices.txt) for full license texts.

## Contributing

Contributions welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines. Since Unreal Engine projects cannot use standard CI, each PR must include proof of a local build and passing tests; maintainers spot-check in the editor before merging.

## Citation

If you use URLab in your research, please cite our [ICRA 2026 paper](https://arxiv.org/abs/2504.14135):

```bibtex
@inproceedings{embleyriches2026urlab,
  title     = {Unreal Robotics Lab: A High-Fidelity Robotics Simulator with Advanced Physics and Rendering},
  author    = {Embley-Riches, Jonathan and Liu, Jianwei and Julier, Simon and Kanoulas, Dimitrios},
  booktitle = {IEEE International Conference on Robotics and Automation (ICRA)},
  year      = {2026},
  url       = {https://arxiv.org/abs/2504.14135}
}
```

## License

Copyright 2026 Jonathan Embley-Riches. Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.

**Disclaimer:** UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere. This plugin incorporates third-party software: MuJoCo (Apache 2.0), CoACD (MIT), and libzmq (MPL 2.0). See [ThirdPartyNotices.txt](ThirdPartyNotices.txt) for details.
