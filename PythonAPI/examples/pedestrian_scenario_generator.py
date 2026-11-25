#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
行人测试场景生成器
基于已有行人系统生成各种测试场景
"""

import carla
import random
import math
import time
import argparse


class PedestrianScenarioGenerator:
    """行人测试场景生成器"""
    
    def __init__(self, host='localhost', port=2000):
        self.client = carla.Client(host, port)
        self.client.set_timeout(10.0)
        self.world = self.client.get_world()
        self.pedestrians = []
        print("[初始化] 行人测试场景生成器已启动")
        
    def load_walker_blueprints(self):
        """加载行人蓝图"""
        blueprint_library = self.world.get_blueprint_library()
        return blueprint_library.filter('walker.pedestrian.*')
    
    def spawn_pedestrian(self, transform, behavior='random'):
        """生成单个行人"""
        walker_bps = self.load_walker_blueprints()
        walker_bp = random.choice(walker_bps)
        
        walker = self.world.spawn_actor(walker_bp, transform)
        
        # 添加AI控制器
        controller_bp = self.world.get_blueprint_library().find('controller.ai.walker')
        controller = self.world.spawn_actor(controller_bp, carla.Transform(), walker)
        
        self.pedestrians.append({'walker': walker, 'controller': controller})
        return walker, controller
    
    def generate_crossing_scenario(self, road_width=10, pedestrian_count=5):
        """生成过马路测试场景"""
        print(f"[场景] 生成过马路场景，道路宽度: {road_width}m，行人: {pedestrian_count}")
        
        # 在道路两侧生成行人
        for i in range(pedestrian_count):
            # 随机位置
            x = random.uniform(-road_width/2, road_width/2)
            y = random.choice([-5, 5])  # 道路两侧
            
            transform = carla.Transform(
                carla.Location(x=x, y=y, z=1.0)
            )
            
            walker, controller = self.spawn_pedestrian(transform)
            
            # 设置过马路目标
            target = carla.Location(x=x, y=-y, z=1.0)
            controller.go_to_location(target)
            controller.set_max_speed(1.4)
            
            print(f"  行人 #{i+1} 从 ({x:.1f}, {y:.1f}) 到 ({x:.1f}, {-y:.1f})")
            
    def generate_crowd_scenario(self, center, radius=20, count=20):
        """生成人群测试场景"""
        print(f"[场景] 生成人群场景，中心: {center}，半径: {radius}m，人数: {count}")
        
        for i in range(count):
            angle = random.uniform(0, 2 * math.pi)
            distance = random.uniform(0, radius)
            
            x = center.x + distance * math.cos(angle)
            y = center.y + distance * math.sin(angle)
            
            transform = carla.Transform(
                carla.Location(x=x, y=y, z=1.0)
            )
            
            walker, controller = self.spawn_pedestrian(transform)
            
            # 随机移动目标
            target_angle = random.uniform(0, 2 * math.pi)
            target_dist = random.uniform(10, 30)
            target = carla.Location(
                x=center.x + target_dist * math.cos(target_angle),
                y=center.y + target_dist * math.sin(target_angle),
                z=1.0
            )
            
            controller.go_to_location(target)
            controller.set_max_speed(random.uniform(1.0, 2.0))
            
    def generate_collision_risk_scenario(self, vehicle_path, pedestrian_count=3):
        """生成碰撞风险测试场景"""
        print(f"[场景] 生成碰撞风险场景，行人: {pedestrian_count}")
        
        # 在车辆路径上生成行人
        for i in range(pedestrian_count):
            # 在路径附近随机位置
            t = random.uniform(0.2, 0.8)  # 路径中间段
            x = vehicle_path[0].x + (vehicle_path[1].x - vehicle_path[0].x) * t
            y = vehicle_path[0].y + (vehicle_path[1].y - vehicle_path[0].y) * t
            
            # 偏移到车道
            offset = random.uniform(-3, 3)
            x += offset
            
            transform = carla.Transform(
                carla.Location(x=x, y=y, z=1.0)
            )
            
            walker, controller = self.spawn_pedestrian(transform)
            
            # 让行人停留在原地或缓慢移动
            if random.choice([True, False]):
                controller.set_max_speed(0.5)  # 缓慢移动
            else:
                controller.set_max_speed(0)  # 静止
                
            print(f"  风险行人 #{i+1} 位置: ({x:.1f}, {y:.1f})")
            
    def cleanup(self):
        """清理所有行人"""
        print(f"[清理] 移除 {len(self.pedestrians)} 个行人")
        for p in self.pedestrians:
            if p['controller'].is_alive:
                p['controller'].stop()
                p['controller'].destroy()
            if p['walker'].is_alive:
                p['walker'].destroy()
        self.pedestrians.clear()


def main():
    parser = argparse.ArgumentParser(description='行人测试场景生成器')
    parser.add_argument('--host', default='localhost', help='主机地址')
    parser.add_argument('--port', type=int, default=2000, help='端口')
    parser.add_argument('--scenario', default='crossing',
                       choices=['crossing', 'crowd', 'risk'],
                       help='场景类型')
    parser.add_argument('--count', type=int, default=5, help='行人数量')
    parser.add_argument('--duration', type=int, default=30, help='持续时间')
    
    args = parser.parse_args()
    
    generator = PedestrianScenarioGenerator(args.host, args.port)
    
    try:
        if args.scenario == 'crossing':
            generator.generate_crossing_scenario(pedestrian_count=args.count)
        elif args.scenario == 'crowd':
            generator.generate_crowd_scenario(
                carla.Location(x=0, y=0, z=0),
                count=args.count
            )
        elif args.scenario == 'risk':
            generator.generate_collision_risk_scenario(
                [carla.Location(x=-50, y=0, z=0), carla.Location(x=50, y=0, z=0)],
                pedestrian_count=args.count
            )
            
        print(f"\n[运行] 场景运行中，持续 {args.duration} 秒...")
        time.sleep(args.duration)
        
    except KeyboardInterrupt:
        print("\n[中断] 用户中断")
    finally:
        generator.cleanup()
        print("[完成] 场景结束")


if __name__ == '__main__':
    main()
