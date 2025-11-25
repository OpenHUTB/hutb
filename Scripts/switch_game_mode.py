# 测试命令：
# python Scripts/switch_game_mode.py -m=vr
# python Scripts/switch_game_mode.py -m=air
import os

import argparse


def switch_game_mode(game_mode="car"):
    # 获取当前脚本所在上一级目录
    cur_dir = os.path.dirname(os.path.abspath(__file__))
    # 获得cur_dir的上一级目录
    pro_dir = os.path.dirname(cur_dir)
    print(pro_dir)
    # 声明用于保存ini文件内容的列表
    out_ini = []
    # 读取.ini文件
    with open(os.path.join(pro_dir, os.path.join(pro_dir, "Unreal", "CarlaUE4", "Config", "DefaultEngine.ini")), "r", encoding="utf-8") as file:
        lines = file.readlines()
        # 查找并替换游戏模式
        for i in range(len(lines)):
            if "GlobalDefaultGameMode=" in lines[i] or "GlobalDefaultServerGameMode=" in lines[i]:
                if game_mode == "vr" and "DReyeVRGameMode" in lines[i]:
                    # 如果以;开始，去掉;
                    if lines[i].strip().startswith(";"):
                        # 去掉前面的空格
                        lines[i] = lines[i].replace(";", "").lstrip()
                        print("正在切换成VR模式...")
                elif game_mode == "vr" and "DReyeVRGameMode" not in lines[i]:
                    # 添加;注释
                    if not lines[i].strip().startswith(";"):
                        lines[i] = ";" + lines[i]
                        print("目标模式为VR，禁用：", lines[i])
                elif game_mode == "air" and "AirSimGameMode" in lines[i]:
                    if lines[i].strip().startswith(";"):
                        lines[i] = lines[i].replace(";", "").lstrip()
                        print("正在切换成Air模式...")
                elif game_mode == "air" and "AirSimGameMode" not in lines[i]:
                    # 添加;注释
                    if not lines[i].strip().startswith(";"):
                        lines[i] = ";" + lines[i]
                        print("目标模式为Air，禁用：", lines[i])
                elif game_mode == "car" and "CarlaGameMode_C" in lines[i]:
                    if lines[i].strip().startswith(";"):
                        lines[i] = lines[i].replace(";", "").lstrip()
                        print("正在切换为Car模式...")
                elif game_mode == "car" and "CarlaGameMode_C" not in lines[i]:
                    # 添加;注释
                    if not lines[i].strip().startswith(";"):
                        lines[i] = ";" + lines[i]
                        print("目标模式为Car，禁用：", lines[i])
                out_ini.append(lines[i])
            else:
                out_ini.append(lines[i])
    # 将out_init列表内容写回.ini文件
    with open(os.path.join(pro_dir, os.path.join(pro_dir, "Unreal", "CarlaUE4", "Config", "DefaultEngine.ini")), "w", encoding="utf-8") as file:
        file.writelines(out_ini)


if __name__ == "__main__":
    argparser = argparse.ArgumentParser()
    argparser.add_argument(
        '-m', '--game_mode',
        default="car",
        help='Package Game Mode (car, vr, air)',)
    args = argparser.parse_args()

    print(args.game_mode)
    # 主函数代码
    switch_game_mode(args.game_mode)
