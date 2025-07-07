# 当关闭车辆物理 set_simulate_physics(False) 时，get_physics_control() 使服务端崩溃
# https://github.com/carla-simulator/carla/issues/8884

# 生成车辆、获得物理控制、运行良好的步骤：
import carla
import random
client = carla.Client()
world = client.get_world()
spawn_points = world.get_map().get_spawn_points()
bp_lib = world.get_blueprint_library()

# 生成车辆
vehicle_bp = bp_lib.find('vehicle.lincoln.mkz_2020')
vehicle = world.try_spawn_actor(vehicle_bp, random.choice(spawn_points))

physics_control = vehicle.get_physics_control()

# 如果关闭物理，则不应该获取物理控制（摩擦系数、轮子位置），否则服务器会崩溃（已修复）
vehicle.set_simulate_physics(False)
physics_control = vehicle.get_physics_control()

# 预期行为
# 无论车辆的物理是否处于活动状态，物理控制都应该可访问，并且不应导致服务器崩溃。

