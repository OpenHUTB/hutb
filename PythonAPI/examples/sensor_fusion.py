#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
多传感器融合模块
融合摄像头、激光雷达、毫米波雷达等多传感器数据
"""

import carla
import numpy as np
import time
import threading
import queue
from collections import deque
import argparse


class SensorData:
    """传感器数据类"""
    def __init__(self, sensor_type, timestamp, data, transform):
        self.sensor_type = sensor_type
        self.timestamp = timestamp
        self.data = data
        self.transform = transform


class SensorFusion:
    """多传感器融合类"""
    
    def __init__(self, host='localhost', port=2000):
        """初始化传感器融合模块"""
        self.client = carla.Client(host, port)
        self.client.set_timeout(10.0)
        self.world = self.client.get_world()
        
        self.sensors = {}
        self.data_buffer = {}
        self.fusion_results = deque(maxlen=100)
        self.running = False
        self.fusion_thread = None
        
        print("[INFO] 多传感器融合模块已初始化")
        
    def setup_sensor_suite(self, vehicle):
        """为车辆配置传感器套件"""
        blueprint_library = self.world.get_blueprint_library()
        
        # 1. RGB摄像头
        camera_bp = blueprint_library.find('sensor.camera.rgb')
        camera_bp.set_attribute('image_size_x', '800')
        camera_bp.set_attribute('image_size_y', '600')
        camera_bp.set_attribute('fov', '110')
        
        camera_transform = carla.Transform(carla.Location(x=1.6, z=1.7))
        camera = self.world.spawn_actor(
            camera_bp, camera_transform, attach_to=vehicle
        )
        camera.listen(lambda data: self._process_camera_data(data, camera))
        self.sensors['camera'] = camera
        self.data_buffer['camera'] = deque(maxlen=10)
        
        # 2. 激光雷达
        lidar_bp = blueprint_library.find('sensor.lidar.ray_cast')
        lidar_bp.set_attribute('range', '100')
        lidar_bp.set_attribute('rotation_frequency', '10')
        lidar_bp.set_attribute('channels', '64')
        lidar_bp.set_attribute('points_per_second', '100000')
        
        lidar_transform = carla.Transform(carla.Location(x=0, z=2.4))
        lidar = self.world.spawn_actor(
            lidar_bp, lidar_transform, attach_to=vehicle
        )
        lidar.listen(lambda data: self._process_lidar_data(data, lidar))
        self.sensors['lidar'] = lidar
        self.data_buffer['lidar'] = deque(maxlen=10)
        
        # 3. 毫米波雷达
        radar_bp = blueprint_library.find('sensor.other.radar')
        radar_bp.set_attribute('horizontal_fov', '30')
        radar_bp.set_attribute('vertical_fov', '30')
        radar_bp.set_attribute('range', '100')
        
        radar_transform = carla.Transform(carla.Location(x=2.0, z=1.0))
        radar = self.world.spawn_actor(
            radar_bp, radar_transform, attach_to=vehicle
        )
        radar.listen(lambda data: self._process_radar_data(data, radar))
        self.sensors['radar'] = radar
        self.data_buffer['radar'] = deque(maxlen=10)
        
        # 4. GNSS
        gnss_bp = blueprint_library.find('sensor.other.gnss')
        gnss_transform = carla.Transform(carla.Location(x=0, z=2.0))
        gnss = self.world.spawn_actor(
            gnss_bp, gnss_transform, attach_to=vehicle
        )
        gnss.listen(lambda data: self._process_gnss_data(data, gnss))
        self.sensors['gnss'] = gnss
        self.data_buffer['gnss'] = deque(maxlen=10)
        
        # 5. IMU
        imu_bp = blueprint_library.find('sensor.other.imu')
        imu_transform = carla.Transform(carla.Location(x=0, z=2.0))
        imu = self.world.spawn_actor(
            imu_bp, imu_transform, attach_to=vehicle
        )
        imu.listen(lambda data: self._process_imu_data(data, imu))
        self.sensors['imu'] = imu
        self.data_buffer['imu'] = deque(maxlen=10)
        
        print("[INFO] 传感器套件配置完成")
        print(f"  - RGB摄像头: {camera.id}")
        print(f"  - 激光雷达: {lidar.id}")
        print(f"  - 毫米波雷达: {radar.id}")
        print(f"  - GNSS: {gnss.id}")
        print(f"  - IMU: {imu.id}")
        
    def _process_camera_data(self, data, sensor):
        """处理摄像头数据"""
        sensor_data = SensorData(
            'camera',
            data.timestamp,
            {
                'frame': data.frame,
                'width': data.width,
                'height': data.height,
                'fov': data.fov
            },
            sensor.get_transform()
        )
        self.data_buffer['camera'].append(sensor_data)
        
    def _process_lidar_data(self, data, sensor):
        """处理激光雷达数据"""
        # 提取点云数据
        points = np.frombuffer(data.raw_data, dtype=np.float32)
        points = np.reshape(points, (int(points.shape[0] / 4), 4))
        
        sensor_data = SensorData(
            'lidar',
            data.timestamp,
            {
                'frame': data.frame,
                'point_count': len(points),
                'points': points
            },
            sensor.get_transform()
        )
        self.data_buffer['lidar'].append(sensor_data)
        
    def _process_radar_data(self, data, sensor):
        """处理毫米波雷达数据"""
        detections = []
        for detection in data:
            detections.append({
                'velocity': detection.velocity,
                'azimuth': detection.azimuth,
                'altitude': detection.altitude,
                'depth': detection.depth
            })
            
        sensor_data = SensorData(
            'radar',
            data.timestamp,
            {
                'frame': data.frame,
                'detection_count': len(detections),
                'detections': detections
            },
            sensor.get_transform()
        )
        self.data_buffer['radar'].append(sensor_data)
        
    def _process_gnss_data(self, data, sensor):
        """处理GNSS数据"""
        sensor_data = SensorData(
            'gnss',
            data.timestamp,
            {
                'latitude': data.latitude,
                'longitude': data.longitude,
                'altitude': data.altitude
            },
            sensor.get_transform()
        )
        self.data_buffer['gnss'].append(sensor_data)
        
    def _process_imu_data(self, data, sensor):
        """处理IMU数据"""
        sensor_data = SensorData(
            'imu',
            data.timestamp,
            {
                'accelerometer': {
                    'x': data.accelerometer.x,
                    'y': data.accelerometer.y,
                    'z': data.accelerometer.z
                },
                'gyroscope': {
                    'x': data.gyroscope.x,
                    'y': data.gyroscope.y,
                    'z': data.gyroscope.z
                },
                'compass': data.compass
            },
            sensor.get_transform()
        )
        self.data_buffer['imu'].append(sensor_data)
        
    def fuse_data(self):
        """执行传感器融合"""
        # 获取最新的传感器数据
        latest_data = {}
        for sensor_type, buffer in self.data_buffer.items():
            if buffer:
                latest_data[sensor_type] = buffer[-1]
                
        if len(latest_data) < 3:
            return None
            
        # 时间同步检查
        timestamps = [d.timestamp for d in latest_data.values()]
        time_diff = max(timestamps) - min(timestamps)
        
        if time_diff > 0.1:  # 100ms阈值
            print(f"[WARNING] 传感器时间不同步: {time_diff*1000:.1f}ms")
            
        # 执行融合
        fusion_result = {
            'timestamp': time.time(),
            'sensor_count': len(latest_data),
            'time_sync_diff': time_diff,
            'objects': [],
            'ego_state': {}
        }
        
        # 融合雷达和激光雷达检测目标
        if 'radar' in latest_data and 'lidar' in latest_data:
            fused_objects = self._fuse_radar_lidar(
                latest_data['radar'].data,
                latest_data['lidar'].data
            )
            fusion_result['objects'] = fused_objects
            
        # 融合定位信息
        if 'gnss' in latest_data and 'imu' in latest_data:
            ego_state = self._fuse_localization(
                latest_data['gnss'].data,
                latest_data['imu'].data
            )
            fusion_result['ego_state'] = ego_state
            
        self.fusion_results.append(fusion_result)
        return fusion_result
        
    def _fuse_radar_lidar(self, radar_data, lidar_data):
        """融合雷达和激光雷达数据"""
        fused_objects = []
        
        # 从激光雷达提取目标
        if lidar_data['point_count'] > 0:
            # 简单的聚类（实际应用中需要更复杂的算法）
            points = lidar_data['points']
            
            # 计算点云的统计特征
            mean_point = np.mean(points[:, :3], axis=0)
            std_point = np.std(points[:, :3], axis=0)
            
            fused_objects.append({
                'source': 'lidar',
                'point_count': lidar_data['point_count'],
                'mean_position': mean_point.tolist(),
                'std_position': std_point.tolist()
            })
            
        # 添加雷达检测
        for detection in radar_data.get('detections', []):
            fused_objects.append({
                'source': 'radar',
                'depth': detection['depth'],
                'velocity': detection['velocity'],
                'azimuth': detection['azimuth']
            })
            
        return fused_objects
        
    def _fuse_localization(self, gnss_data, imu_data):
        """融合定位信息"""
        # 简单的传感器融合（实际应用中使用卡尔曼滤波）
        ego_state = {
            'position': {
                'latitude': gnss_data['latitude'],
                'longitude': gnss_data['longitude'],
                'altitude': gnss_data['altitude']
            },
            'acceleration': imu_data['accelerometer'],
            'angular_velocity': imu_data['gyroscope'],
            'heading': imu_data['compass']
        }
        
        return ego_state
        
    def start_fusion(self, frequency=10):
        """启动融合线程"""
        self.running = True
        interval = 1.0 / frequency
        
        def fusion_loop():
            while self.running:
                result = self.fuse_data()
                if result:
                    print(f"[融合] 检测到 {len(result['objects'])} 个目标")
                time.sleep(interval)
                
        self.fusion_thread = threading.Thread(target=fusion_loop)
        self.fusion_thread.start()
        print(f"[INFO] 融合线程已启动，频率: {frequency}Hz")
        
    def stop_fusion(self):
        """停止融合"""
        self.running = False
        if self.fusion_thread:
            self.fusion_thread.join()
        print("[INFO] 融合线程已停止")
        
    def get_fusion_report(self):
        """获取融合报告"""
        if not self.fusion_results:
            return "没有融合数据"
            
        latest = self.fusion_results[-1]
        
        report = f"""
========== 传感器融合报告 ==========
时间戳: {latest['timestamp']:.3f}
传感器数量: {latest['sensor_count']}
时间同步差异: {latest['time_sync_diff']*1000:.1f}ms

检测目标: {len(latest['objects'])}
"""
        for i, obj in enumerate(latest['objects'][:5]):  # 只显示前5个
            report += f"  [{i+1}] 来源: {obj.get('source', 'unknown')}\n"
            if 'point_count' in obj:
                report += f"      点数: {obj['point_count']}\n"
            if 'depth' in obj:
                report += f"      距离: {obj['depth']:.2f}m\n"
                
        if latest['ego_state']:
            report += f"\n自车状态:\n"
            report += f"  位置: ({latest['ego_state']['position']['latitude']:.6f}, "
            report += f"{latest['ego_state']['position']['longitude']:.6f})\n"
            report += f"  航向: {latest['ego_state']['heading']:.2f}°\n"
            
        report += "="*40
        return report
        
    def cleanup(self):
        """清理资源"""
        self.stop_fusion()
        
        for sensor in self.sensors.values():
            if sensor.is_alive:
                sensor.stop()
                sensor.destroy()
        self.sensors.clear()
        
        print("[INFO] 传感器资源已清理")


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='多传感器融合模块')
    parser.add_argument('--host', default='localhost', help='模拟器主机地址')
    parser.add_argument('--port', type=int, default=2000, help='模拟器端口')
    parser.add_argument('--frequency', type=int, default=10, help='融合频率(Hz)')
    
    args = parser.parse_args()
    
    fusion = SensorFusion(args.host, args.port)
    
    try:
        # 获取车辆
        vehicles = fusion.world.get_actors().filter('vehicle.*')
        if vehicles:
            vehicle = vehicles[0]
            print(f"[INFO] 找到车辆: {vehicle.type_id}")
            
            # 配置传感器
            fusion.setup_sensor_suite(vehicle)
            
            # 启动融合
            fusion.start_fusion(args.frequency)
            
            print("[INFO] 按 Ctrl+C 停止")
            while True:
                time.sleep(2)
                print(fusion.get_fusion_report())
        else:
            print("[WARNING] 未找到车辆")
            
    except KeyboardInterrupt:
        print("\n[INFO] 程序被中断")
    finally:
        fusion.cleanup()


if __name__ == '__main__':
    main()
