# OpenHUTB 本地水下 ROV 仿真使用与操作手册 (本地开发版)

本手册专为本地开发与测试人员编写，指导如何在本地 `OpenHUTB-hutb/hutb` 模拟器中启动、操控与验证水下机器人（ROV）及水下场景。

---

## 快速导航：三步进入水下仿真

```
 [步骤 1: 一键拉起]               [步骤 2: 视口与运行]              [步骤 3: 键盘航行与视角]
双击 Open_Underwater_Editor.bat ──> 视口显示 Underwater_TestBed ──> 按 [W/S/A/D/Space/C] 航行
自动分发 settings_rov.json           点击顶部绿色【Play (运行)】        按 [V] 键切换双目水下相机
```

---

## 第一步：一键启动虚幻编辑器 (UE4Editor)

在 Windows 资源管理器中，进入 `E:\Projects\OpenHUTB-hutb\hutb` 目录：
1. **直接双击运行**：`Open_Underwater_Editor.bat`
   - *（或者在 PowerShell 运行：`.\Open_Underwater_Editor.bat`）*
2. **脚本自动完成的事项**：
   - 自动将水下机器人配置 `settings_rov.json` 同步至 `%USERPROFILE%\Documents\AirSim\settings.json`；
   - 自动检测本地引擎 `E:\Projects\hutb_editor\hutb_editor\unreal\Engine\Binaries\Win64\UE4Editor.exe`；
   - 自动载入水下专属测试关卡 `Underwater_TestBed.umap` 并打开编辑器。

---

## 第二步：在编辑器中查看与运行

当虚幻编辑器窗口加载完成后：
1. **视口预览**：
   - 您将看到专属的水下测试场：上层为水面，下层为海底大陆架地形与水下工程管道，中央悬浮着开架式水下机器人（BlueROV2 Heavy 模型）；
   - 水体呈现清澈水下散射与指数高度雾光影效果。
2. **启动物理仿真**：
   - 点击虚幻引擎编辑器顶部主工具栏的 **绿色三角【Play (运行)】** 按钮（或快捷键 `Alt + P`）；
   - 模拟器进入运行状态，ROV 激活中性浮力平衡机制。

---

## 第三步：键盘按键操作指令表

在游戏运行窗口中点击鼠标左键获取输入焦点，即可使用键盘操控 ROV：

| 按键 (Key) | 动作 (Action) | 物理表现与现象 |
| :---: | :--- | :--- |
| **W** | 前进 (Forward) | 水平推进器推力喷涌，ROV 平稳向前破水航行 |
| **S** | 后退 (Backward) | 水平推进器反转，ROV 匀速倒车后退 |
| **A** | 左平移 (Strafe Left) | ROV 保持首向不变，向左侧平移横向机动 |
| **D** | 右平移 (Strafe Right) | ROV 保持首向不变，向右侧平移横向机动 |
| **Space (空格)** | 垂直上浮 (Ascend) | 4 轴垂直推进器正转，ROV 垂直向上升起 |
| **C** | 垂直下潜 (Descend) | 4 轴垂直推进器反转，ROV 垂直向下深潜 |
| **Q** | 左转偏航 (Yaw Left) | ROV 逆时针就地打转偏航 |
| **E** | 右转偏航 (Yaw Right) | ROV 顺时针就地打转偏航 |
| **0 ~ 4 / V** | 切换相机视角 (Cameras) | 在前视中心双目、前右、前左、下视声呐相机与第三人称跟随视角之间切换 |
| **F** | 视口自由跟随 (Fly Camera) | 切换自由巡检摄像机或第一人称水下机器人视角 |
| **Esc** | 退出运行模式 (Stop) | 退出 Play 状态，返回关卡编辑视口 |

---

## 第四步：如何直观验证您已研发的核心功能？

### 1. 验证“中性浮力与自稳恢复力矩”
- **操作**：按 `W` 加速向前开行，或轻微调整俯仰角后松开所有按键；
- **现象**：松开按键后，ROV **绝对不会**像以前一样沉底摔入虚空！在浮力与初稳心（COB-COM 偏置力矩）作用下，ROV 会在 1~2 秒内自动水平修正姿态，稳稳地悬浮在当前水深层。

### 2. 验证“水深软硬安全网（海底防坠兜底）”
- **操作**：按住 `C` 键强行持续向下深潜，直逼海底设定极值（$Z = -30\,\text{m}$）；
- **现象**：当接近设定海底极值时，弹簧-阻尼接触安全网立即生效，产生柔和支撑力阻挡下潜，即使按住下潜键也绝对无法穿透下界，彻底杜绝掉入虚空掉出世界。

### 3. 验证水下传感器与双目视觉数据流
- 在本地打开另一个 PowerShell 窗口，运行 AirSim Python 客户端检查传感器数据：
  ```powershell
  python -c "import airsim; c = airsim.VehicleClient(); c.confirmConnection(); print('ROV 状态:', c.getMultirotorState().kinematics_estimated.position)"
  ```
  您将在终端中看到实时的毫米级深度、线速度与姿态四元数更新。

---

## 常见问题与排查 (Troubleshooting)

1. **点击 Play 后无法用键盘操纵？**
   - **原因**：鼠标焦点仍在编辑器面板上。
   - **解决**：在运行画面的水体视口中鼠标**单击一下**，将输入焦点锁定在游戏视口内。
2. **想要在真实的 Town10 城市海湾测试？**
   - 运行批处理时传入地图名即可：
     ```cmd
     .\Open_Underwater_Editor.bat Town10HD
     ```
