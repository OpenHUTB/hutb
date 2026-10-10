#!/usr/bin/env python3
"""CarlaAir Quick Start - Step 3: 生成交通"""
import carla
import random
import time
import argparse

# 解析命令行参数
# 参数	                 作用
# '--host'	            长选项名,命令行写 --host 192.168.1.5 或 --host=192.168.1.5
# metavar='H'	        只影响 --help 里的显示(显示成 --host H),并不会因此多出一个 
                        # -H 短选项 —— 这是最常见的误解
# default='localhost'	不带该参数时的默认值
# help=...	            --help 时显示的说明文字
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

# 把前面登记好的规则(默认值、type=int 等)
# 应用到真实的命令行参数上,返回一个装了最终值的对象,存进 args。
# 没传参数        → args = Namespace(host='localhost', port=2000)
# --port 3654    → args = Namespace(host='localhost', port=3654)
# 敲 -h/--help   → 这一步直接打印帮助并退出,后面代码不执行
args = argparser.parse_args()

# CARLA 客户端的标准"握手"流程：连接模拟器 → 拿到世界对象 → 拿到蓝图库
# 1.client创建一个客户端对象，绑定到前面命令行参数解析出的地址。
# 注意这一步还没有真正联网——CARLA 的 Client 构造是惰性的，
# 只是记下 IP/端口，真正的 TCP 连接发生在第一次发起 RPC 调用时（即下一行）。

# 2.world = client.get_world() 向服务器请求当前的 world 对象，
# 这里是真正建立连接的一步（连接不上会超时抛 RuntimeError）。
# 这里获取的不是"某个文件"或"某张地图"，而是 CARLA 服务器端正在运行的那一整场仿真
# 包括地图、场上所有车/人/传感器、天气、时间在内的整个"当前局面"，在客户端这边的遥控器。

# 3.bp_lib取回蓝图库——一份"能生成的 actor 模板"总目录，
# 包含 vehicle.*、walker.pedestrian.*、sensor.* 等条目。
# 蓝图本身不是场景里的实物，而是一张配置表（车型、颜色、是否无敌等属性）；
# 要生成实物，得先挑一张蓝图再交给 try_spawn_actor。
client = carla.Client(args.host, args.port)
world = client.get_world()
bp_lib = world.get_blueprint_library()

# 在城市中心附近生成车辆 (x > 55, 远离海岸)
# 1.spawn_points拿到地图里所有预定义的生成点
# 2.city_spawns用列表推导式筛选出 x 坐标大于 55 的点，取前 10 个作为生成点
spawn_points = world.get_map().get_spawn_points()
city_spawns = [sp for sp in spawn_points if sp.location.x > 55][:10]

# 1.bp 随即挑一辆车的蓝图
# 2.如果该蓝图有 color 属性，就随机挑一个颜色值设置进去
# 3.调用 world.try_spawn_actor(bp, sp) 尝试在指定生成点生成车辆，成功返回车辆对象 v，失败返回 None
# 4.如果生成成功，就把车辆设置为自动驾驶模式，并把车辆对象存进 vehicles 列表
vehicles = []
for sp in city_spawns:
    bp = random.choice(bp_lib.filter('vehicle.*'))
    if bp.has_attribute('color'):
        bp.set_attribute('color', random.choice(bp.get_attribute('color').recommended_values))
    v = world.try_spawn_actor(bp, sp)
    if v:
        v.set_autopilot(True)
        vehicles.append(v)
print(f"{len(vehicles)} autonomous vehicles have been spawned.")

# 生成静态行人
walkers = []
for _ in range(15):
    loc = world.get_random_location_from_navigation()
    if loc:
        bp = random.choice(bp_lib.filter('walker.pedestrian.*'))
        if bp.has_attribute('is_invincible'):
            bp.set_attribute('is_invincible', 'true')
        w = world.try_spawn_actor(bp, carla.Transform(loc))
        if w:
            walkers.append(w)
print(f"{len(walkers)} pedestrians have been spawned.")

print("Observing traffic... (20 seconds)")
time.sleep(20)

for v in vehicles: v.destroy()
for w in walkers: w.destroy()
print("Cleanup completed. All actors have been destroyed.")
