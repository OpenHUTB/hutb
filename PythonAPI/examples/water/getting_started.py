import holoocean
import numpy as np

env = holoocean.make("PierHarbor-Hovering")

# 悬停的AUV对每个推进器发出指令
command = np.array([10,10,10,10,0,0,0,0])

for _ in range(1800):
   state = env.step(command)