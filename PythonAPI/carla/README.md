这是 OpenHUTB 模拟器 的 Python API 包，用于控制和与 [OpenHUTB](https://github.com/OpenHUTB/hutb)（用于人车研究的开源模拟器）进行通信。

此包允许您控制 OpenHUTB 模拟器并通过 Python API 检索模拟数据。例如，您可以控制模拟中的任何参与者（人、车、无人机、交通信号灯等），将传感器连接到参与者，并读取传感器数据等。

## 开发

```shell
cd hutb/PythonAPI/carla
# 实现编辑模式安装（注意关闭代理）
# 修改 setup.py 后需要再次运行才会生效
pip install -e .
# 测试 API
python ../util/config.py -l
python ../examples/water/getting_started.py
```

更多信息，请参阅 [OpenHUTB 文档](https://openhutb.github.io/) 。
