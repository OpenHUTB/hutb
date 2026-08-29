#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
记录数据分析工具
基于已有记录器数据进行分析
"""

import carla
import json
import os
import argparse
from datetime import datetime


class RecorderDataAnalyzer:
    """记录数据分析器"""
    
    def __init__(self, recording_file=None):
        self.recording_file = recording_file
        self.data = None
        
    def load_recording(self, file_path):
        """加载记录文件"""
        if not os.path.exists(file_path):
            print(f"[错误] 文件不存在: {file_path}")
            return False
            
        try:
            with open(file_path, 'r') as f:
                self.data = json.load(f)
            print(f"[信息] 已加载记录文件: {file_path}")
            return True
        except Exception as e:
            print(f"[错误] 加载失败: {e}")
            return False
            
    def analyze_vehicle_trajectory(self):
        """分析车辆轨迹"""
        if not self.data:
            return None
            
        positions = []
        for frame in self.data.get('frames', []):
            for actor in frame.get('actors', []):
                if actor.get('type_id', '').startswith('vehicle'):
                    pos = actor.get('location', {})
                    positions.append((pos.get('x', 0), pos.get('y', 0), pos.get('z', 0)))
                    
        if not positions:
            return None
            
        # 计算总距离
        total_distance = 0
        for i in range(1, len(positions)):
            dx = positions[i][0] - positions[i-1][0]
            dy = positions[i][1] - positions[i-1][1]
            dz = positions[i][2] - positions[i-1][2]
            total_distance += (dx**2 + dy**2 + dz**2)**0.5
            
        return {
            'total_points': len(positions),
            'total_distance': total_distance,
            'start_point': positions[0],
            'end_point': positions[-1]
        }
        
    def analyze_collision_events(self):
        """分析碰撞事件"""
        if not self.data:
            return None
            
        collisions = []
        for event in self.data.get('events', []):
            if event.get('type') == 'collision':
                collisions.append({
                    'time': event.get('time', 0),
                    'actors': event.get('actors', []),
                    'intensity': event.get('intensity', 0)
                })
                
        return {
            'total_collisions': len(collisions),
            'collisions': collisions
        }
        
    def generate_report(self, output_file=None):
        """生成分析报告"""
        if not self.data:
            print("[错误] 没有加载数据")
            return
            
        if not output_file:
            output_file = f"analysis_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            
        report = {
            'generated_at': datetime.now().isoformat(),
            'source_file': self.recording_file,
            'trajectory_analysis': self.analyze_vehicle_trajectory(),
            'collision_analysis': self.analyze_collision_events()
        }
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
            
        print(f"[信息] 分析报告已保存: {output_file}")
        return report


def main():
    parser = argparse.ArgumentParser(description='记录数据分析工具')
    parser.add_argument('--file', required=True, help='记录文件路径')
    parser.add_argument('--output', help='输出报告路径')
    
    args = parser.parse_args()
    
    analyzer = RecorderDataAnalyzer(args.file)
    if analyzer.load_recording(args.file):
        analyzer.generate_report(args.output)


if __name__ == '__main__':
    main()
