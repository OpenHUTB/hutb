#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
交通灯测试工具
基于已有交通灯系统进行功能测试和验证
"""

import carla
import time
import argparse
from enum import Enum


class TrafficLightTester:
    """交通灯测试器"""
    
    def __init__(self, host='localhost', port=2000):
        self.client = carla.Client(host, port)
        self.client.set_timeout(10.0)
        self.world = self.client.get_world()
        self.traffic_lights = []
        print("[初始化] 交通灯测试工具已启动")
        
    def get_all_traffic_lights(self):
        """获取所有交通灯"""
        self.traffic_lights = self.world.get_actors().filter('traffic.traffic_light')
        print(f"[信息] 发现 {len(self.traffic_lights)} 个交通灯")
        return self.traffic_lights
    
    def test_traffic_light_states(self):
        """测试交通灯状态切换"""
        print("\n[测试] 交通灯状态测试")
        self.get_all_traffic_lights()
        
        for i, tl in enumerate(self.traffic_lights[:5]):  # 测试前5个
            print(f"\n  交通灯 #{i+1} (ID: {tl.id}):")
            print(f"    当前状态: {tl.get_state()}")
            print(f"    绿灯时间: {tl.get_green_time():.1f}s")
            print(f"    黄灯时间: {tl.get_yellow_time():.1f}s")
            print(f"    红灯时间: {tl.get_red_time():.1f}s")
            print(f"    冻结状态: {tl.is_frozen()}")
            
    def test_state_transition(self, duration=30):
        """测试状态转换"""
        print(f"\n[测试] 交通灯状态转换测试，持续 {duration} 秒")
        self.get_all_traffic_lights()
        
        if not self.traffic_lights:
            print("[警告] 未找到交通灯")
            return
            
        tl = self.traffic_lights[0]
        start_time = time.time()
        state_changes = []
        last_state = tl.get_state()
        
        print(f"  监控交通灯 #{tl.id} 的状态变化...")
        
        try:
            while time.time() - start_time < duration:
                current_state = tl.get_state()
                if current_state != last_state:
                    change_time = time.time() - start_time
                    state_changes.append({
                        'time': change_time,
                        'from': last_state,
                        'to': current_state
                    })
                    print(f"    [{change_time:.1f}s] {last_state} -> {current_state}")
                    last_state = current_state
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            pass
            
        print(f"\n[结果] 共检测到 {len(state_changes)} 次状态切换")
        return state_changes
    
    def test_manual_control(self):
        """测试手动控制"""
        print("\n[测试] 交通灯手动控制测试")
        self.get_all_traffic_lights()
        
        if not self.traffic_lights:
            return
            
        tl = self.traffic_lights[0]
        original_state = tl.get_state()
        
        print(f"  原始状态: {original_state}")
        
        # 测试设置为红灯
        tl.set_state(carla.TrafficLightState.Red)
        print(f"  设置为红灯: {tl.get_state()}")
        time.sleep(2)
        
        # 测试设置为绿灯
        tl.set_state(carla.TrafficLightState.Green)
        print(f"  设置为绿灯: {tl.get_state()}")
        time.sleep(2)
        
        # 恢复原始状态
        tl.set_state(original_state)
        print(f"  恢复原始状态: {tl.get_state()}")
        
    def generate_test_report(self):
        """生成测试报告"""
        self.get_all_traffic_lights()
        
        report = {
            'total_traffic_lights': len(self.traffic_lights),
            'traffic_lights': []
        }
        
        for tl in self.traffic_lights:
            report['traffic_lights'].append({
                'id': tl.id,
                'state': str(tl.get_state()),
                'green_time': tl.get_green_time(),
                'yellow_time': tl.get_yellow_time(),
                'red_time': tl.get_red_time(),
                'frozen': tl.is_frozen()
            })
            
        return report


def main():
    parser = argparse.ArgumentParser(description='交通灯测试工具')
    parser.add_argument('--host', default='localhost', help='主机地址')
    parser.add_argument('--port', type=int, default=2000, help='端口')
    parser.add_argument('--test', default='all',
                       choices=['all', 'states', 'transition', 'manual'],
                       help='测试类型')
    
    args = parser.parse_args()
    
    tester = TrafficLightTester(args.host, args.port)
    
    if args.test == 'all' or args.test == 'states':
        tester.test_traffic_light_states()
        
    if args.test == 'all' or args.test == 'transition':
        tester.test_state_transition(duration=30)
        
    if args.test == 'all' or args.test == 'manual':
        tester.test_manual_control()
        
    print("\n[完成] 测试结束")


if __name__ == '__main__':
    main()
