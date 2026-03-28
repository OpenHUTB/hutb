# config.py
# 全局配置参数

# CARLA 服务器配置
CARLA_HOST = 'localhost'
CARLA_PORT = 2000
CARLA_TIMEOUT = 10.0

# 传感器配置
IMAGE_WIDTH = 160
IMAGE_HEIGHT = 80
CAMERA_FOV = 90
CAMERA_LOCATION = (1.5, 0.0, 2.4)  # 相对于车辆的位置 (x, y, z)
CAMERA_ROTATION = (0.0, 0.0, 0.0)  # 俯仰, 偏航, 横滚

# 数据收集配置
COLLECT_FRAMES = 1000          # 每次收集多少帧
COLLECT_SAVE_DIR = './data'    # 数据保存根目录

# 训练配置
BATCH_SIZE = 64
EPOCHS = 50
LEARNING_RATE = 0.0001
TRAIN_VAL_SPLIT = 0.8          # 训练集比例
MODEL_SAVE_PATH = './pretrained/model.pth'

# 模型输入尺寸 (C, H, W)
INPUT_SHAPE = (3, IMAGE_HEIGHT, IMAGE_WIDTH)
OUTPUT_DIM = 2                 # steer, throttle