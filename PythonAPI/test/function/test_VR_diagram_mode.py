import carla
import sys
import os

# 导入 ../../examples/ 目录下的 DReyeVR_utils.py 中的模块
# 获取当前脚本所在的目录
current_dir = os.path.dirname(os.path.abspath(__file__))
# 获取 ../../examples/ 目录的绝对路径
examples_dir = os.path.abspath(os.path.join(current_dir, "../../examples"))
sys.path.append(examples_dir)
from DReyeVR_utils import find_ego_vehicle, DReyeVRSensor


# 指定 CARLA 服务器的 IP 地址和端口号
CARLA_SERVER_IP = "localhost"
CARLA_SERVER_PORT = 3654
client = carla.Client(CARLA_SERVER_IP, CARLA_SERVER_PORT)
world = client.get_world()

# 找到自主车辆
hero_actor = find_ego_vehicle(world)
if hero_actor is None:
    raise Exception("No DReyeVR EgoVehicle found in the world!")
else:
    print(f"Found DReyeVR EgoVehicle: {hero_actor}")
# hero_transform = hero_actor.get_transform()

# 找到 ego 传感器
# DReyeVRSensor 隐式调用 find_ego_sensor，然后用自定义类包装它
# sensor = DReyeVRSensor(world) 
# sensor.ego_sensor.listen(sensor.update)  # 订阅读出 readout