#!/usr/bin/env python3
"""CarlaAir Quick Start - Step 1: 连接验证"""
import carla
import airsim
import argparse

argparser = argparse.ArgumentParser(
    description=__doc__)
argparser.add_argument(
    '--host',
    metavar='H',
    default='localhost',
    help='IP of the host CARLA Simulator (default: localhost)')
argparser.add_argument(
    '-p', '--port',
    metavar='P',
    default=2000,
    type=int,
    help='TCP port of CARLA Simulator (default: 2000)')

args = argparser.parse_args()

# 连接 CARLA（地面仿真）
client = carla.Client(args.host, args.port)
client.set_timeout(10)
world = client.get_world()
print(f"CARLA 连接成功: {world.get_map().name}")
print(f"可用生成点: {len(world.get_map().get_spawn_points())} 个")

# 连接 AirSim（空中仿真）
air = airsim.MultirotorClient(port=41451)
air.confirmConnection()
print("AirSim 连接成功: 无人机就绪")

weather = world.get_weather()
print(f"当前天气: 太阳高度={weather.sun_altitude_angle:.1f}°, 云量={weather.cloudiness:.1f}%")
print("\nCarlaAir 已就绪！地面 + 空中 API 均可用。")
