import time

import carla
import random

# 连接到客户端并获取世界对象
client = carla.Client('localhost', 2000)
world = client.get_world()

# 我们还将设置观察者，这样我们就可以看到我们在做什么
# 如果在编辑器中运行，需要是用“独立进程运行”，否则 world.get_spectator() 报错：
# RuntimeError: internal error: unable to find spectator
spectator = world.get_spectator()

spawn_points = world.get_map().get_spawn_points()

# 将观察者移动到俯视图
transform = carla.Transform(spawn_points[0].transform(carla.Location(x=-4, z=100)), carla.Rotation(yaw=-180, pitch=-90))
spectator.set_transform(transform)

debug_helper = world.debug

# 在地图上用数字标出生成点的位置
for i, spawn_point in enumerate(spawn_points):
    debug_helper.draw_string(spawn_point.location, str(i), life_time=15)  #


debug = world.debug
world_snapshot = world.get_snapshot()

for actor_snapshot in world_snapshot:
    actual_actor = world.get_actor(actor_snapshot.id)
    if actual_actor.type_id == 'traffic.traffic_light':
        debug.draw_box(carla.BoundingBox(actor_snapshot.get_transform().location,carla.Vector3D(0.5,0.5,2)), actor_snapshot.get_transform().rotation, 0.05, carla.Color(255, 0, 0, 0), 0)
# ...

print("start clear string")
# 清除调式时的绘制信息
debug_helper.clear_debug_string()
time.sleep(3)
print("clear finished")
