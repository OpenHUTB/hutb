#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
性能监控仪表盘
基于已有性能分析功能提供可视化监控
"""

import carla
import time
import psutil
import json
from datetime import datetime
import argparse


class PerformanceDashboard:
    """性能监控仪表盘"""
    
    def __init__(self, host='localhost', port=2000):
        self.client = carla.Client(host, port)
        self.client.set_timeout(10.0)
        self.world = self.client.get_world()
        self.running = False
        
    def get_system_metrics(self):
        """获取系统指标"""
        return {
            'cpu_percent': psutil.cpu_percent(interval=0.1),
            'memory_percent': psutil.virtual_memory().percent,
            'memory_used_gb': psutil.virtual_memory().used / (1024**3),
            'disk_percent': psutil.disk_usage('/').percent
        }
        
    def get_simulator_metrics(self):
        """获取模拟器指标"""
        actors = self.world.get_actors()
        return {
            'total_actors': len(actors),
            'vehicles': len(actors.filter('vehicle.*')),
            'pedestrians': len(actors.filter('walker.*')),
            'sensors': len(actors.filter('sensor.*'))
        }
        
    def print_dashboard(self):
        """打印仪表盘"""
        sys_metrics = self.get_system_metrics()
        sim_metrics = self.get_simulator_metrics()
        
        print("\n" + "="*60)
        print(f"性能监控仪表盘 - {datetime.now().strftime('%H:%M:%S')}")
        print("="*60)
        
        print("\n[系统资源]")
        print(f"  CPU使用率: {sys_metrics['cpu_percent']:.1f}%")
        print(f"  内存使用: {sys_metrics['memory_percent']:.1f}% ({sys_metrics['memory_used_gb']:.2f} GB)")
        print(f"  磁盘使用: {sys_metrics['disk_percent']:.1f}%")
        
        print("\n[模拟器状态]")
        print(f"  总演员数: {sim_metrics['total_actors']}")
        print(f"  车辆数: {sim_metrics['vehicles']}")
        print(f"  行人数: {sim_metrics['pedestrians']}")
        print(f"  传感器数: {sim_metrics['sensors']}")
        
        # 性能评估
        issues = []
        if sys_metrics['cpu_percent'] > 80:
            issues.append("CPU使用率过高")
        if sys_metrics['memory_percent'] > 80:
            issues.append("内存使用率过高")
        if sim_metrics['total_actors'] > 200:
            issues.append("演员数量过多")
            
        if issues:
            print("\n[警告]")
            for issue in issues:
                print(f"  ! {issue}")
        else:
            print("\n[状态] 运行正常")
            
        print("="*60)
        
    def run_dashboard(self, interval=5.0):
        """运行仪表盘"""
        self.running = True
        print(f"[信息] 启动性能监控仪表盘，刷新间隔: {interval}秒")
        print("[信息] 按 Ctrl+C 停止")
        
        try:
            while self.running:
                self.print_dashboard()
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\n[信息] 已停止")
            
    def export_metrics(self, duration=60, output_file=None):
        """导出性能指标"""
        if not output_file:
            output_file = f"performance_metrics_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            
        metrics_history = []
        start_time = time.time()
        
        print(f"[信息] 收集性能指标，持续 {duration} 秒")
        
        while time.time() - start_time < duration:
            metrics_history.append({
                'timestamp': datetime.now().isoformat(),
                'system': self.get_system_metrics(),
                'simulator': self.get_simulator_metrics()
            })
            time.sleep(1)
            
        with open(output_file, 'w') as f:
            json.dump(metrics_history, f, indent=2)
            
        print(f"[信息] 指标已保存: {output_file}")
        return output_file


def main():
    parser = argparse.ArgumentParser(description='性能监控仪表盘')
    parser.add_argument('--host', default='localhost', help='主机地址')
    parser.add_argument('--port', type=int, default=2000, help='端口')
    parser.add_argument('--mode', default='dashboard',
                       choices=['dashboard', 'export'],
                       help='模式')
    parser.add_argument('--interval', type=float, default=5.0, help='刷新间隔')
    parser.add_argument('--duration', type=int, default=60, help='导出时长')
    
    args = parser.parse_args()
    
    dashboard = PerformanceDashboard(args.host, args.port)
    
    if args.mode == 'dashboard':
        dashboard.run_dashboard(args.interval)
    elif args.mode == 'export':
        dashboard.export_metrics(args.duration)


if __name__ == '__main__':
    main()
