#!/usr/bin/env python3
# main_collect_data.py
import carla
import numpy as np
import cv2
import argparse
import time
from pathlib import Path
import random
from config import *
from utils import spawn_vehicle, spawn_camera, control_to_array, process_image

def main():
    parser = argparse.ArgumentParser(description='Collect data from CARLA')
    parser.add_argument('--host', default=CARLA_HOST, help='CARLA server host')
    parser.add_argument('--port', default=CARLA_PORT, type=int, help='CARLA server port')
    parser.add_argument('--save_dir', default=COLLECT_SAVE_DIR, help='Directory to save data')
    parser.add_argument('--frames', default=COLLECT_FRAMES, type=int, help='Number of frames to collect')
    parser.add_argument('--autopilot', action='store_true', help='Use CARLA autopilot for collection')
    args = parser.parse_args()

    # 连接CARLA
    client = carla.Client(args.host, args.port)
    client.set_timeout(CARLA_TIMEOUT)
    world = client.get_world()
    blueprint_library = world.get_blueprint_library()

    # 生成车辆
    vehicle = spawn_vehicle(world, blueprint_library)
    if args.autopilot:
        vehicle.set_autopilot(True)

    # 生成摄像头
    camera = spawn_camera(world, blueprint_library, vehicle,
                          width=IMAGE_WIDTH, height=IMAGE_HEIGHT,
                          fov=CAMERA_FOV,
                          location=CAMERA_LOCATION,
                          rotation=CAMERA_ROTATION)

    # 准备保存路径
    save_path = Path(args.save_dir)
    save_path.mkdir(parents=True, exist_ok=True)
    images_dir = save_path / 'images'
    images_dir.mkdir(exist_ok=True)

    # 数据缓存
    images = []
    actions = []

    # 回调函数
    def on_image(image):
        # 处理图像
        img = process_image(image)
        # 获取当前车辆控制信号
        control = vehicle.get_control()
        action = control_to_array(control)
        # 存储
        img_filename = images_dir / f'{image.frame}.png'
        cv2.imwrite(str(img_filename), cv2.cvtColor(img, cv2.COLOR_RGB2BGR))
        actions.append(action)
        print(f'Collected frame {image.frame}', end='\r')

    # 监听摄像头
    camera.listen(on_image)

    try:
        print(f'Collecting {args.frames} frames...')
        for _ in range(args.frames):
            world.tick()
        print('\nCollection finished.')
    except KeyboardInterrupt:
        print('\nInterrupted by user.')
    finally:
        # 保存动作数据
        np.save(save_path / 'actions.npy', np.array(actions))
        camera.destroy()
        vehicle.destroy()
        print(f'Data saved to {save_path}')

if __name__ == '__main__':
    main()