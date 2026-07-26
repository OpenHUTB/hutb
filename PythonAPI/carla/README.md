这是 HUTB Python API 的 Python 包，用于控制和与 [HUTB](https://github.com/OpenHUTB/hutb)（用于人车研究的开源模拟器）进行通信。

此包允许您控制 HUTB 模拟器并通过 Python API 检索模拟数据。例如，您可以控制模拟中的任何参与者（人、车、无人机、交通信号灯等），将传感器连接到参与者，并读取传感器数据等。

## 开发

```shell
cd hutb/PythonAPI/carla
# 实现编辑模式安装
# 修改 setup.py 后需要再次运行才会生效
pip install -e .
# 测试 API
python ../util/config.py -l
python ../examples/holoocean/getting_started.py
```

更多信息，请参阅 [HUTB 文档](https://openhutb.github.io/) 。
