#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
路径规划可视化工具
可视化车辆路径规划结果，包括全局路径和局部轨迹
"""

import carla
import math
import time
import argparse
from collections import deque


class PathVisualizer:
    """路径可视化器"""
    
    def __init__(self, host='localhost', port=2000):
        """初始化路径可视化器"""
        self.client = carla.Client(host, port)
        self.client.set_timeout(10.0)
        self.world = self.client.get_world()
        self.debug = self.world.debug
        self.path_points = []
        self.trajectory_history = deque(maxlen=1000)
        self.waypoints = []
        print("[INFO] 路径可视化器已初始化")
        
    def draw_waypoints(self, waypoints, color=None, life_time=60.0):
        """绘制路径点"""
        if color is None:
            color = carla.Color(0, 255, 0)  # 绿色
            
        for i, waypoint in enumerate(waypoints):
            # 绘制点
            self.debug.draw_point(
                waypoint.transform.location + carla.Location(z=0.5),
                size=0.1,
                color=color,
                life_time=life_time
            )
            
            # 绘制箭头表示方向
            if i < len(waypoints) - 1:
                next_loc = waypoints[i + 1].transform.location
                self.debug.draw_arrow(
                    waypoint.transform.location + carla.Location(z=0.5),
                    next_loc + carla.Location(z=0.5),
                    thickness=0.1,
                    arrow_size=0.3,
                    color=color,
                    life_time=life_time
                )
                
    def draw_trajectory(self, trajectory, color=None, life_time=10.0):
        """绘制轨迹"""
        if color is None:
            color = carla.Color(255, 0, 0)  # 红色
            
        for i in range(len(trajectory) - 1):
            self.debug.draw_line(
                trajectory[i] + carla.Location(z=0.5),
                trajectory[i + 1] + carla.Location(z=0.5),
                thickness=0.2,
                color=color,
                life_time=life_time
            )
            
    def draw_path_from_map(self, start_waypoint, distance=100.0):
        """从地图绘制路径"""
        print(f"[INFO] 从地图生成路径，距离: {distance}m")
        
        waypoints = []
        current = start_waypoint
        
        for _ in range(int(distance / 2)):  # 每2米一个点
            waypoints.append(current)
            next_wps = current.next(2.0)
            if not next_wps:
                break
            current = next_wps[0]
            
        self.waypoints = waypoints
        self.draw_waypoints(waypoints, carla.Color(0, 255, 0))
        print(f"[INFO] 生成了 {len(waypoints)} 个路径点")
        return waypoints
        
    def draw_lane_change(self, waypoint, direction='left', distance=30.0):
        """绘制变道路径"""
        if direction == 'left':
            target_lane = waypoint.get_left_lane()
            color = carla.Color(0, 0, 255)  # 蓝色
        else:
            target_lane = waypoint.get_right_lane()
            color = carla.Color(255, 255, 0)  # 黄色
            
        if not target_lane:
            print(f"[WARNING] 无法变道到{direction}")
            return None
            
        # 绘制变道轨迹（简化的曲线）
        start_loc = waypoint.transform.location
        end_loc = target_lane.transform.location
        
        # 生成贝塞尔曲线点
        trajectory = []
        steps = 20
        for i in range(steps + 1):
            t = i / steps
            # 二次贝塞尔曲线
            mid_loc = carla.Location(
                x=(start_loc.x + end_loc.x) / 2 + 5,  # 控制点偏移
                y=(start_loc.y + end_loc.y) / 2,
                z=(start_loc.z + end_loc.z) / 2
            )
            
            # 计算曲线点
            loc = carla.Location(
                x=(1-t)**2 * start_loc.x + 2*(1-t)*t * mid_loc.x + t**2 * end_loc.x,
                y=(1-t)**2 * start_loc.y + 2*(1-t)*t * mid_loc.y + t**2 * end_loc.y,
                z=(1-t)**2 * start_loc.z + 2*(1-t)*t * mid_loc.z + t**2 * end_loc.z
            )
            trajectory.append(loc)
            
        self.draw_trajectory(trajectory, color)
        print(f"[INFO] 绘制了{direction}变道路径")
        return trajectory
        
    def draw_route(self, start_location, end_location, color=None):
        """绘制从起点到终点的路线"""
        if color is None:
            color = carla.Color(255, 165, 0)  # 橙色
            
        # 绘制起点和终点
        self.debug.draw_point(
            start_location + carla.Location(z=1.0),
            size=0.3,
            color=carla.Color(0, 255, 0),  # 绿色起点
            life_time=60.0
        )
        
        self.debug.draw_point(
            end_location + carla.Location(z=1.0),
            size=0.3,
            color=carla.Color(255, 0, 0),  # 红色终点
            life_time=60.0
        )
        
        # 绘制连线
        self.debug.draw_line(
            start_location + carla.Location(z=1.0),
            end_location + carla.Location(z=1.0),
            thickness=0.3,
            color=color,
            life_time=60.0
        )
        
        # 计算距离
        distance = math.sqrt(
            (end_location.x - start_location.x)**2 +
            (end_location.y - start_location.y)**2
        )
        
        # 在中点显示距离
        mid_loc = carla.Location(
            x=(start_location.x + end_location.x) / 2,
            y=(start_location.y + end_location.y) / 2,
            z=(start_location.z + end_location.z) / 2 + 2.0
        )
        
        self.debug.draw_string(
            mid_loc,
            f"{distance:.1f}m",
            color=carla.Color(255, 255, 255),
            life_time=60.0
        )
        
        print(f"[INFO] 绘制路线，距离: {distance:.1f}m")
        
    def track_vehicle(self, vehicle, duration=30.0):
        """跟踪车辆并绘制轨迹"""
        print(f"[INFO] 开始跟踪车辆 {vehicle.id}，持续 {duration} 秒")
        
        start_time = time.time()
        trajectory = []
        
        try:
            while time.time() - start_time < duration:
                location = vehicle.get_location()
                trajectory.append(location)
                self.trajectory_history.append(location)
                
                # 实时绘制轨迹
                if len(trajectory) > 1:
                    self.debug.draw_line(
                        trajectory[-2] + carla.Location(z=0.5),
                        trajectory[-1] + carla.Location(z=0.5),
                        thickness=0.2,
                        color=carla.Color(255, 0, 0),
                        life_time=5.0
                    )
                    
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\n[INFO] 跟踪被中断")
            
        print(f"[INFO] 跟踪结束，记录了 {len(trajectory)} 个点")
        return trajectory
        
    def draw_collision_box(self, vehicle, color=None, life_time=0.1):
        """绘制车辆碰撞框"""
        if color is None:
            color = carla.Color(255, 255, 0)  # 黄色
            
        transform = vehicle.get_transform()
        bounding_box = vehicle.bounding_box
        
        # 绘制边界框
        self.debug.draw_box(
            bounding_box,
            transform.rotation,
            thickness=0.1,
            color=color,
            life_time=life_time
        )
        
    def visualize_speed(self, vehicle):
        """可视化车辆速度"""
        velocity = vehicle.get_velocity()
        speed = 3.6 * math.sqrt(velocity.x**2 + velocity.y**2 + velocity.z**2)
        
        location = vehicle.get_location()
        text_loc = location + carla.Location(z=2.5)
        
        # 根据速度选择颜色
        if speed < 30:
            color = carla.Color(0, 255, 0)  # 绿色
        elif speed < 60:
            color = carla.Color(255, 255, 0)  # 黄色
        else:
            color = carla.Color(255, 0, 0)  # 红色
            
        self.debug.draw_string(
            text_loc,
            f"{speed:.1f} km/h",
            color=color,
            life_time=0.1
        )
        
    def clear_all(self):
        """清除所有可视化"""
        # 在CARLA中，debug绘制会自动过期
        # 这里只是重置内部状态
        self.path_points.clear()
        self.trajectory_history.clear()
        self.waypoints.clear()
        print("[INFO] 已清除所有可视化状态")


def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='路径规划可视化工具')
    parser.add_argument('--host', default='localhost', help='模拟器主机地址')
    parser.add_argument('--port', type=int, default=2000, help='模拟器端口')
    parser.add_argument('--mode', default='waypoints',
                       choices=['waypoints', 'track', 'route', 'lane_change'],
                       help='可视化模式')
    
    args = parser.parse_args()
    
    visualizer = PathVisualizer(args.host, args.port)
    
    try:
        if args.mode == 'waypoints':
            # 获取当前地图的第一个路径点
            map = visualizer.world.get_map()
            spawn_points = map.get_spawn_points()
            if spawn_points:
                start_wp = map.get_waypoint(spawn_points[0].location)
                visualizer.draw_path_from_map(start_wp, distance=100.0)
            
        elif args.mode == 'track':
            # 跟踪第一辆车
            vehicles = visualizer.world.get_actors().filter('vehicle.*')
            if vehicles:
                visualizer.track_vehicle(vehicles[0], duration=30.0)
            else:
                print("[WARNING] 未找到车辆")
                
        elif args.mode == 'route':
            # 绘制从(0,0)到(100,0)的路线
            start = carla.Location(x=0, y=0, z=0)
            end = carla.Location(x=100, y=0, z=0)
            visualizer.draw_route(start, end)
            
        elif args.mode == 'lane_change':
            map = visualizer.world.get_map()
            spawn_points = map.get_spawn_points()
            if spawn_points:
                start_wp = map.get_waypoint(spawn_points[0].location)
                visualizer.draw_lane_change(start_wp, 'left')
                
        print("[INFO] 可视化完成，按 Ctrl+C 退出")
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n[INFO] 程序被中断")
    finally:
        visualizer.clear_all()


if __name__ == '__main__':
    main()
