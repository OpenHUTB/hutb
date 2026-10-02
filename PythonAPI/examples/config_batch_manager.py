#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
配置批量管理工具
基于已有配置系统进行批量管理
"""

import carla
import json
import os
import argparse
from datetime import datetime


class ConfigBatchManager:
    """配置批量管理器"""
    
    def __init__(self, host='localhost', port=2000, config_dir='./configs'):
        self.client = carla.Client(host, port)
        self.client.set_timeout(10.0)
        self.world = self.client.get_world()
        self.config_dir = config_dir
        
        if not os.path.exists(config_dir):
            os.makedirs(config_dir)
            
    def export_current_config(self, name=None):
        """导出当前配置"""
        if not name:
            name = f"config_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            
        config = {
            'timestamp': datetime.now().isoformat(),
            'map': self.world.get_map().name,
            'weather': self._get_weather_dict(),
            'settings': self._get_settings_dict()
        }
        
        filepath = os.path.join(self.config_dir, f"{name}.json")
        with open(filepath, 'w') as f:
            json.dump(config, f, indent=2)
            
        print(f"[信息] 配置已导出: {filepath}")
        return filepath
        
    def _get_weather_dict(self):
        """获取天气字典"""
        w = self.world.get_weather()
        return {
            'cloudiness': w.cloudiness,
            'precipitation': w.precipitation,
            'wind_intensity': w.wind_intensity,
            'fog_density': w.fog_density,
            'wetness': w.wetness
        }
        
    def _get_settings_dict(self):
        """获取设置字典"""
        s = self.world.get_settings()
        return {
            'synchronous_mode': s.synchronous_mode,
            'fixed_delta_seconds': s.fixed_delta_seconds
        }
        
    def batch_export(self, count=5, interval=10):
        """批量导出配置"""
        print(f"[信息] 批量导出 {count} 个配置，间隔 {interval} 秒")
        
        for i in range(count):
            self.export_current_config(f"batch_{i+1}")
            if i < count - 1:
                time.sleep(interval)
                
    def list_configs(self):
        """列出所有配置"""
        configs = []
        for f in os.listdir(self.config_dir):
            if f.endswith('.json'):
                configs.append(f[:-5])
        return configs
        
    def compare_configs(self, config1, config2):
        """比较两个配置"""
        filepath1 = os.path.join(self.config_dir, f"{config1}.json")
        filepath2 = os.path.join(self.config_dir, f"{config2}.json")
        
        with open(filepath1, 'r') as f:
            c1 = json.load(f)
        with open(filepath2, 'r') as f:
            c2 = json.load(f)
            
        differences = {}
        for key in set(c1.keys()) | set(c2.keys()):
            if c1.get(key) != c2.get(key):
                differences[key] = {
                    'config1': c1.get(key),
                    'config2': c2.get(key)
                }
                
        return differences


def main():
    parser = argparse.ArgumentParser(description='配置批量管理工具')
    parser.add_argument('--host', default='localhost', help='主机地址')
    parser.add_argument('--port', type=int, default=2000, help='端口')
    parser.add_argument('--action', default='export',
                       choices=['export', 'batch', 'list', 'compare'],
                       help='操作')
    parser.add_argument('--name', help='配置名称')
    parser.add_argument('--config1', help='比较配置1')
    parser.add_argument('--config2', help='比较配置2')
    
    args = parser.parse_args()
    
    manager = ConfigBatchManager(args.host, args.port)
    
    if args.action == 'export':
        manager.export_current_config(args.name)
    elif args.action == 'batch':
        manager.batch_export()
    elif args.action == 'list':
        configs = manager.list_configs()
        print("[信息] 可用配置:")
        for c in configs:
            print(f"  - {c}")
    elif args.action == 'compare':
        if args.config1 and args.config2:
            diff = manager.compare_configs(args.config1, args.config2)
            print(json.dumps(diff, indent=2))


if __name__ == '__main__':
    main()
