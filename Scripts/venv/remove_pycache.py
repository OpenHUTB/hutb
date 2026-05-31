import os
import shutil

for root, dirs, files in os.walk('.'):
    if '__pycache__' in dirs:
        path = os.path.join(root, '__pycache__')
        print(f"正在删除: {path}")
        shutil.rmtree(path)
