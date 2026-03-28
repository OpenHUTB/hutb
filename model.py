# model.py
import torch
import torch.nn as nn
import torch.nn.functional as F

class End2EndModel(nn.Module):
    """端到端自动驾驶模型，输入图像，输出转向和油门"""
    def __init__(self, input_shape=(3, 80, 160), output_dim=2):
        super().__init__()
        self.input_shape = input_shape
        # 卷积层
        self.conv = nn.Sequential(
            nn.Conv2d(3, 24, kernel_size=5, stride=2),
            nn.ReLU(),
            nn.Conv2d(24, 36, kernel_size=5, stride=2),
            nn.ReLU(),
            nn.Conv2d(36, 48, kernel_size=5, stride=2),
            nn.ReLU(),
            nn.Conv2d(48, 64, kernel_size=3, stride=1),
            nn.ReLU(),
            nn.Conv2d(64, 64, kernel_size=3, stride=1),
            nn.ReLU(),
        )
        # 计算卷积输出尺寸
        self._to_linear = None
        self._get_conv_output(input_shape)

        # 全连接层
        self.fc = nn.Sequential(
            nn.Linear(self._to_linear, 100),
            nn.ReLU(),
            nn.Linear(100, 50),
            nn.ReLU(),
            nn.Linear(50, output_dim)
        )

    def _get_conv_output(self, shape):
        """计算经过卷积层后的展平维度"""
        with torch.no_grad():
            dummy = torch.zeros(1, *shape)
            dummy = self.conv(dummy)
            self._to_linear = dummy.view(1, -1).size(1)

    def forward(self, x):
        # 归一化到 [0,1]（假设输入已经是 0-255 的 uint8 张量）
        x = x.float() / 255.0
        x = self.conv(x)
        x = x.view(x.size(0), -1)
        x = self.fc(x)
        # 输出：转向用 tanh 限制在 [-1,1]，油门用 sigmoid 限制在 [0,1]
        steer = torch.tanh(x[:, 0:1])
        throttle = torch.sigmoid(x[:, 1:2])
        return torch.cat([steer, throttle], dim=1)