import os
import time

import msgpack
import msgpack_numpy

from robojudo.config.g1.env.g1_dummy_env_cfg import G1DummyEnvCfg
from robojudo.environment.dummy_env import DummyEnv
from robojudo.tools.tool_cfgs import ForwardKinematicCfg

msgpack_numpy.patch()


def get_latest_folder(path, index=-1):
    folder_list = os.listdir(path)
    folder_list = [os.path.join(path, f) for f in folder_list]
    folder_list = list(filter(lambda f: os.path.isdir(f), folder_list))
    folder_list.sort(key=lambda f: os.path.getmtime(f))
    return folder_list[index]


def read_msgpack(folder_path):
    log_files = sorted([f for f in os.listdir(folder_path) if f.endswith(".msgpack")])
    if not log_files:
        exit()
    log_file = log_files[0]
    path = os.path.join(folder_path, log_file)

    records = []
    with open(path, "rb") as f:
        unpacker = msgpack.Unpacker(f, raw=False)
        for obj in unpacker:
            records.append(obj)
    return records


def view_log(folder_path):
    log_frames = read_msgpack(folder_path)
    log_frames = log_frames[:]

    fk_cfg = ForwardKinematicCfg(
        xml_path=G1DummyEnvCfg.model_fields["xml"].default,
        debug_viz=True,
    )
    env_cfg = G1DummyEnvCfg(forward_kinematic=fk_cfg, odometry_type="DUMMY")

    env = DummyEnv(cfg_env=env_cfg)

    for _i, log_frame in enumerate(log_frames):
        env_data = log_frame["env_data"]
        ctrl_data = log_frame["ctrl_data"]
        extras = log_frame["extras"]
        pd_target = log_frame["pd_target"]
        timestep = log_frame["timestep"]
        time_then = log_frame["time"]

        joint_pos = env_data["dof_pos"]
        base_pos = env_data["base_pos"]
        base_quat = env_data["base_quat"]
        env.kinematics.forward(
            joint_pos=joint_pos,
            base_pos=base_pos,
            base_quat=base_quat,
        )
        for command in ctrl_data.get("COMMANDS", []):
            print("----->" + command)
        print(f"Step {timestep}")
        time.sleep(0.005)


def plot_log(folder_path):
    log_frames = read_msgpack(folder_path)
    log_frames = log_frames[1000:]

    plot_data = [
        [0],
        [],
    ]
    for _i, log_frame in enumerate(log_frames):
        env_data = log_frame["env_data"]
        ctrl_data = log_frame["ctrl_data"]
        extras = log_frame["extras"]
        pd_target = log_frame["pd_target"]
        timestep = log_frame["timestep"]
        time_then = log_frame["time"]

        joint_pos = env_data["dof_pos"]
        base_pos = env_data["base_pos"]
        base_quat = env_data["base_quat"]

        plot_data[0].append(joint_pos[13])  # waist roll
        plot_data[1].append(pd_target[13])

    import matplotlib.pyplot as plt

    fig, ax1 = plt.subplots(figsize=(12, 6))

    ax1.plot(plot_data[0], color="blue", label="DOF Position")
    ax1.set_xlabel("Index", fontsize=12)
    ax1.set_ylabel("DOF Position", color="blue", fontsize=12)
    ax1.tick_params(axis="y", labelcolor="blue")
    ax1.grid()

    ax2 = ax1.twinx()
    ax2.plot(plot_data[1], color="red", label="PD TARGET")
    ax2.set_ylabel("PD target", color="red", fontsize=12)
    ax2.tick_params(axis="y", labelcolor="red")
    fig.legend(loc="upper right", bbox_to_anchor=(0.9, 0.9), fontsize=10)

    plt.title("DOF Position", fontsize=16)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    folder_path = get_latest_folder("logs", -1)
    print(folder_path)
    # plot_log(folder_path)
    view_log(folder_path)
