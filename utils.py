# utils.py
import numpy as np
import cv2
import carla
import random

def process_image(image):
    """将Carla传感器图像转为numpy数组 (H,W,3) RGB格式，并resize至目标尺寸"""
    img = np.array(image.raw_data).reshape(image.height, image.width, 4)[:, :, :3]  # RGBA -> RGB
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    img = cv2.resize(img, (160, 80))
    return img

def control_to_array(control):
    """将carla.VehicleControl转为numpy数组 [steer, throttle, brake]"""
    return np.array([control.steer, control.throttle, control.brake], dtype=np.float32)

def array_to_control(arr):
    """将numpy数组 [steer, throttle] 转为carla.VehicleControl"""
    control = carla.VehicleControl()
    control.steer = float(np.clip(arr[0], -1.0, 1.0))
    control.throttle = float(np.clip(arr[1], 0.0, 1.0))
    control.brake = 0.0
    return control

def spawn_vehicle(world, blueprint_library, spawn_point=None):
    """生成车辆并返回"""
    vehicle_bp = blueprint_library.filter('vehicle.tesla.model3')[0]
    if spawn_point is None:
        spawn_point = random.choice(world.get_map().get_spawn_points())
    vehicle = world.spawn_actor(vehicle_bp, spawn_point)
    return vehicle

def spawn_camera(world, blueprint_library, vehicle, width=160, height=80, fov=90, location=(1.5,0,2.4), rotation=(0,0,0)):
    """生成RGB摄像头并附加到车辆"""
    camera_bp = blueprint_library.find('sensor.camera.rgb')
    camera_bp.set_attribute('image_size_x', str(width))
    camera_bp.set_attribute('image_size_y', str(height))
    camera_bp.set_attribute('fov', str(fov))
    transform = carla.Transform(
        carla.Location(x=location[0], y=location[1], z=location[2]),
        carla.Rotation(pitch=rotation[0], yaw=rotation[1], roll=rotation[2])
    )
    camera = world.spawn_actor(camera_bp, transform, attach_to=vehicle)
    return camera