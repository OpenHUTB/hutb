# ZED Proxy

@GDDG08

- proxy layer for zed mini camera
- to run on Nvidia Jetson Pack Seperately

```bash
conda create -n zed-proxy python=3.12
conda activate zed-proxy

conda install -c conda-forge gcc
python /usr/local/zed/get_python_api.py
```