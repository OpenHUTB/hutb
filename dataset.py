# dataset.py
import torch
from torch.utils.data import Dataset, DataLoader
import numpy as np
import cv2
from pathlib import Path
import random

class CarlaDataset(Dataset):
    """CARLA数据集，加载图像和对应的控制信号"""
    def __init__(self, data_dir, transform=None, val=False, train_split=0.8):
        self.data_dir = Path(data_dir)
        self.transform = transform
        # 加载动作数据
        self.actions = np.load(self.data_dir / 'actions.npy')
        # 获取所有图像文件并排序
        self.image_files = sorted(self.data_dir.glob('images/*.png'))
        assert len(self.image_files) == len(self.actions), "图像与动作数量不匹配"
        # 划分训练/验证集
        split_idx = int(len(self.image_files) * train_split)
        if val:
            self.image_files = self.image_files[split_idx:]
            self.actions = self.actions[split_idx:]
        else:
            self.image_files = self.image_files[:split_idx]
            self.actions = self.actions[:split_idx]

    def __len__(self):
        return len(self.image_files)

    def __getitem__(self, idx):
        # 读取图像
        img = cv2.imread(str(self.image_files[idx]))
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)  # 转为RGB
        # 确保尺寸一致（若数据收集时已统一则可跳过，但保险起见）
        img = cv2.resize(img, (160, 80))
        # 获取动作 [steer, throttle, brake] -> 只用前两个
        action = self.actions[idx][:2].astype(np.float32)

        if self.transform:
            img = self.transform(img)
        else:
            # 默认转换：HWC -> CHW, 并转为 float32
            img = torch.from_numpy(img.transpose(2, 0, 1)).float()

        return img, action