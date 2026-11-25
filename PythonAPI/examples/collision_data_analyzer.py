#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
碰撞数据分析工具
基于已有碰撞检测数据进行分析
"""

import carla
import json
import math
import argparse
from datetime import datetime


class CollisionDataAnalyzer:
    """碰撞数据分析器"""
    
    def __init__(self, host='localhost', port=2000):
        self.client = carla.Client(host, port)
        self.client.set_timeout(10.0)
        self.world = self.client.get_world()
        self.collision_events = []
        
    def collect_collision_data(self, duration=60):
        """收集碰撞数据"""
        print(f"[信息] 开始收集碰撞数据，持续 {duration} 秒")
        
        vehicles = self.world.get_actors().filter('vehicle.*')
        if not vehicles:
            print("[警告] 未找到车辆")
            return
            
        vehicle = vehicles[0]
        
        # 附加碰撞传感器
        blueprint_library = self.world.get_blueprint_library()
        collision_bp = blueprint_library.find('sensor.other.collision')
        collision_sensor = self.world.spawn_actor(
            collision_bp, carla.Transform(), attach_to=vehicle
        )
        
        def on_collision(event):
            self.collision_events.append({
                'timestamp': datetime.now().isoformat(),
                'actor': event.actor.type_id,
                'other_actor': event.other_actor.type_id if event.other_actor else None,
                'impulse': {
                    'x': event.normal_impulse.x,
                    'y': event.normal_impulse.y,
                    'z': event.normal_impulse.z
                },
                'intensity': math.sqrt(
                    event.normal_impulse.x**2 +
                    event.normal_impulse.y**2 +
                    event.normal_impulse.z**2
                )
            })
            
        collision_sensor.listen(on_collision)
        
        try:
            time.sleep(duration)
        except KeyboardInterrupt:
            pass
        finally:
            collision_sensor.destroy()
            
        print(f"[信息] 收集完成，共 {len(self.collision_events)} 次碰撞")
        
    def analyze_collision_patterns(self):
        """分析碰撞模式"""
        if not self.collision_events:
            return None
            
        # 统计碰撞强度分布
        intensity_levels = {'轻微': 0, '中等': 0, '严重': 0}
        for event in self.collision_events:
            intensity = event['intensity']
            if intensity < 100:
                intensity_levels['轻微'] += 1
            elif intensity < 500:
                intensity_levels['中等'] += 1
            else:
                intensity_levels['严重'] += 1
                
        return {
            'total_collisions': len(self.collision_events),
            'intensity_distribution': intensity_levels,
            'average_intensity': sum(e['intensity'] for e in self.collision_events) / len(self.collision_events)
        }
        
    def generate_report(self, output_file=None):
        """生成碰撞分析报告"""
        analysis = self.analyze_collision_patterns()
        
        if not output_file:
            output_file = f"collision_analysis_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            
        report = {
            'generated_at': datetime.now().isoformat(),
            'analysis': analysis,
            'events': self.collision_events
        }
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
            
        print(f"[信息] 分析报告已保存: {output_file}")
        return report


def main():
    parser = argparse.ArgumentParser(description='碰撞数据分析工具')
    parser.add_argument('--host', default='localhost', help='主机地址')
    parser.add_argument('--port', type=int, default=2000, help='端口')
    parser.add_argument('--duration', type=int, default=60, help='收集时长')
    parser.add_argument('--output', help='输出文件')
    
    args = parser.parse_args()
    
    analyzer = CollisionDataAnalyzer(args.host, args.port)
    analyzer.collect_collision_data(args.duration)
    analyzer.generate_report(args.output)


if __name__ == '__main__':
    main()
