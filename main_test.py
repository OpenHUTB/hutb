#!/usr/bin/env python3
# main_test.py
import carla
import torch
import numpy as np
import cv2
import argparse
import time
import random
from config import *
from model import End2EndModel
from utils import spawn_vehicle, spawn_camera, array_to_control, process_image

def main():
    parser = argparse.ArgumentParser(description='Test end-to-end model in CARLA')
    parser.add_argument('--model_path', required=True, help='Path to trained model')
    parser.add_argument('--host', default=CARLA_HOST, help='CARLA server host')
    parser.add_argument('--port', default=CARLA_PORT, type=int, help='CARLA server port')
    args = parser.parse_args()

    # 加载模型
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = End2EndModel(input_shape=INPUT_SHAPE, output_dim=OUTPUT_DIM)
    model.load_state_dict(torch.load(args.model_path, map_location=device))
    model.to(device)
    model.eval()

    # 连接CARLA
    client = carla.Client(args.host, args.port)
    client.set_timeout(CARLA_TIMEOUT)
    world = client.get_world()
    blueprint_library = world.get_blueprint_library()

    # 生成车辆和摄像头
    vehicle = spawn_vehicle(world, blueprint_library)
    camera = spawn_camera(world, blueprint_library, vehicle,
                          width=IMAGE_WIDTH, height=IMAGE_HEIGHT,
                          fov=CAMERA_FOV,
                          location=CAMERA_LOCATION,
                          rotation=CAMERA_ROTATION)

    # 控制变量
    def on_image(image):
        # 处理图像
        img = process_image(image)
        # 模型推理
        tensor = torch.from_numpy(img.transpose(2, 0, 1)).float().unsqueeze(0).to(device)  # [1,3,H,W]
        with torch.no_grad():
            output = model(tensor).squeeze().cpu().numpy()  # [steer, throttle]
        # 应用控制
        control = array_to_control(output)
        vehicle.apply_control(control)
        # 显示图像（可选）
        cv2.imshow('Model View', img)
        cv2.waitKey(1)

    camera.listen(on_image)

    print('Testing... Press Ctrl+C to exit.')
    try:
        while True:
            world.tick()
            time.sleep(0.05)  # 避免过高负载
    except KeyboardInterrupt:
        print('\nTest stopped.')
    finally:
        camera.destroy()
        vehicle.destroy()
        cv2.destroyAllWindows()

if __name__ == '__main__':
    main()