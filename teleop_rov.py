import sys
import time
import os

# Ensure AirSim python client is in path
sys.path.append(r"E:\Projects\OpenHUTB-air\air\PythonClient")
import airsim
import carla

def main():
    print("=" * 65)
    print("     OpenHUTB 水下机器人 (ROV) 键盘实时遥控器 (Teleop)")
    print("=" * 65)
    print("正在连接 AirSim 物理引擎与 CARLA 视景服务器...")

    rov = airsim.RovClient()
    rov.confirmConnection()
    rov.enableApiControl(True)
    rov.armDisarm(True)
    print("[OK] AirSim ROV 动力学已连接并解锁 (Armed)！")

    carla_client = carla.Client("localhost", 2000)
    carla_client.set_timeout(3.0)
    world = carla_client.get_world()
    spectator = world.get_spectator()
    print("[OK] CARLA 视景服务器已连接！")

    # 1. 自动将 ROV 与摄像机瞬移至 Town10 海湾水下
    print("\n[INFO] 正在将 ROV 部署至 Town10 蔚蓝海湾水下 (-2.0m 深度)...")
    target_pos = carla.Location(x=150.0, y=100.0, z=-2.0)
    
    # 查找 airsim drone/rov actor
    rov_actor = None
    for a in world.get_actors():
        if "airsim" in a.type_id.lower() or "rov" in a.type_id.lower():
            rov_actor = a
            break
            
    if rov_actor:
        rov_actor.set_location(target_pos)
        cam_loc = carla.Location(x=144.0, y=100.0, z=-0.5)
        spectator.set_transform(carla.Transform(cam_loc, carla.Rotation(pitch=-15, yaw=0, roll=0)))
        print("[OK] ROV 与摄像机已成功入水！请看虚幻编辑器窗口！")
    else:
        print("[WARN] 未在 CARLA 中直接匹配到 ROV Actor，将依赖 AirSim 局部坐标。")

    print("\n" + "=" * 65)
    print("【操作指引】请在本终端中直接按键操控 ROV (无需回车)：")
    print("  [W] 前进 (Forward)        [S] 后退 (Backward)")
    print("  [A] 左转 (Yaw Left)       [D] 右转 (Yaw Right)")
    print("  [空格] 垂直上浮 (Ascend)  [C] 垂直下潜 (Descend)")
    print("  [X] 紧急刹车悬停 (Stop)   [Q] 退出遥控 (Quit)")
    print("=" * 65)

    import msvcrt

    # 推进器配置:
    # 0~3: 垂直推进器
    # 4~7: 水平矢量推进器 (4:前右, 5:前左, 6:后左, 7:后右)
    while True:
        if msvcrt.kbhit():
            ch = msvcrt.getch()
            try:
                key = ch.decode('utf-8').lower()
            except Exception:
                continue

            pwms = [0.0] * 8
            action_name = ""

            if key == 'w':
                # 前进: 前右推、前左推、后左反推、后右反推
                pwms = [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, -1.0, -1.0]
                action_name = "前进 (Forward)"
            elif key == 's':
                # 后退
                pwms = [0.0, 0.0, 0.0, 0.0, -1.0, -1.0, 1.0, 1.0]
                action_name = "后退 (Backward)"
            elif key == 'a':
                # 逆时针左偏航转弯
                pwms = [0.0, 0.0, 0.0, 0.0, -1.0, 1.0, 1.0, -1.0]
                action_name = "左转 (Turn Left)"
            elif key == 'd':
                # 顺时针右偏航转弯
                pwms = [0.0, 0.0, 0.0, 0.0, 1.0, -1.0, -1.0, 1.0]
                action_name = "右转 (Turn Right)"
            elif key == ' ':
                # 垂直上浮
                pwms = [-1.0, -1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 0.0]
                action_name = "上浮 (Ascend)"
            elif key == 'c':
                # 垂直下潜
                pwms = [1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0]
                action_name = "下潜 (Descend)"
            elif key == 'x':
                pwms = [0.0] * 8
                action_name = "定深自稳悬停 (Hover)"
            elif key == 'q':
                print("\n正在释放控制权并退出...")
                rov.moveByMotorPWMsAsync([0.0] * 8, 0.1).join()
                rov.armDisarm(False)
                rov.enableApiControl(False)
                print("遥控已安全结束。")
                break
            else:
                continue

            # 发送推进器推力并持续 0.4 秒
            rov.moveByMotorPWMsAsync(pwms, 0.4)

            # 获取状态
            state = rov.getRovState()
            pos = state.kinematics_estimated.position
            vel = state.kinematics_estimated.linear_velocity
            
            # 同步摄像机跟随 ROV
            if rov_actor:
                curr_loc = rov_actor.get_location()
                cam_loc = carla.Location(x=curr_loc.x - 5.0, y=curr_loc.y, z=curr_loc.z + 1.5)
                spectator.set_transform(carla.Transform(cam_loc, carla.Rotation(pitch=-15, yaw=0, roll=0)))

            print(f"\r[{action_name}] 深度 Z: {pos.z_val:+.2f}m | 速度 Vx: {vel.x_val:+.2f}m/s, Vz: {vel.z_val:+.2f}m/s   ", end="", flush=True)

        time.sleep(0.05)

if __name__ == "__main__":
    main()
