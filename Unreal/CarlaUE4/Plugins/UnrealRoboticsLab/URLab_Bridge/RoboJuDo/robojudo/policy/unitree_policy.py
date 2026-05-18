import numpy as np

from robojudo.environment.utils.mujoco_viz import MujocoVisualizer
from robojudo.policy import Policy, policy_registry
from robojudo.policy.policy_cfgs import UnitreePolicyCfg, UnitreeWoGaitPolicyCfg
from robojudo.utils.util_func import command_remap, get_gravity_orientation


@policy_registry.register
class UnitreePolicy(Policy):
    cfg_policy: UnitreePolicyCfg

    def __init__(self, cfg_policy, device):
        super().__init__(cfg_policy=cfg_policy, device=device)

        self.obs_scales = self.cfg_policy.obs_scales
        self.max_cmd = self.cfg_policy.max_cmd
        self.commands_map = self.cfg_policy.commands_map

        self.reset()

    def reset(self):
        self.timestep: int = 0

        self._init_history(np.zeros(self.history_obs_size))

    def post_step_callback(self, commands=None):
        self.timestep += 1

    def _get_phase(self):
        cycle_time = 0.8
        phase = self.timestep * self.dt / cycle_time
        return phase

    def _get_commands(self, ctrl_data):
        commands = np.zeros(3)
        for key in ctrl_data.keys():
            if key in ["JoystickCtrl", "UnitreeCtrl"]:
                axes = ctrl_data[key]["axes"]
                lx, ly, rx, ry = axes["LeftX"], axes["LeftY"], axes["RightX"], axes["RightY"]

                commands[0] = command_remap(ly, self.commands_map[0])
                commands[1] = command_remap(lx, self.commands_map[1])
                commands[2] = command_remap(rx, self.commands_map[2])
                break
            if key in ["KeyboardCtrl"]:
                keys = ctrl_data[key]["keyboard_event"]
                for event in keys:
                    if event["type"] == "keyboard":
                        value = event["pressed"] * 1.5
                        match event["name"]:
                            case "w":
                                commands[0] = command_remap(value, self.commands_map[0])
                            case "s":
                                commands[0] = command_remap(-value, self.commands_map[0])
                            case "a":
                                commands[1] = command_remap(-value, self.commands_map[1])
                            case "d":
                                commands[1] = command_remap(value, self.commands_map[1])
                            case "e":
                                commands[2] = command_remap(value, self.commands_map[2])
                            case "q":
                                commands[2] = command_remap(-value, self.commands_map[2])
                break
        return commands

    def get_observation(self, env_data, ctrl_data):
        phase = self._get_phase()
        commands = self._get_commands(ctrl_data)

        sin_pos = [np.sin(2 * np.pi * phase)]
        cos_pos = [np.cos(2 * np.pi * phase)]

        gravity_orientation = get_gravity_orientation(env_data.base_quat)
        obs = np.concatenate(
            [
                env_data.base_ang_vel * self.obs_scales.ang_vel,
                gravity_orientation,
                commands * self.obs_scales.command * self.max_cmd,
                env_data.dof_pos - self.default_dof_pos,
                env_data.dof_vel * self.obs_scales.dof_vel,
                self.last_action,
                sin_pos,
                cos_pos,
            ]
        )

        extras = {
            "phase": phase,
            "commands": commands,
        }
        return obs, extras

    def debug_viz(self, visualizer: MujocoVisualizer, env_data, ctrl_data, extras):
        base_pos = env_data["base_pos"]
        base_quat = env_data["base_quat"]
        command_x = extras["commands"][0]
        command_y = extras["commands"][1]
        command_yaw = extras["commands"][2]

        visualizer.draw_arrow(
            base_pos,
            base_quat,
            [command_x, 0, 0],
            color=[1, 0, 0, 1],
            scale=2,
            horizontal_only=True,
            id=0,
        )
        visualizer.draw_arrow(
            base_pos,
            base_quat,
            [0, command_y, 0],
            color=[0, 1, 0, 1],
            scale=2,
            horizontal_only=True,
            id=1,
        )
        visualizer.draw_arrow(
            base_pos + np.array([0.0, 0, 0.6]),
            base_quat,
            [0, command_yaw, 0],
            color=[1, 1, 1, 1],
            scale=2,
            horizontal_only=True,
            id=2,
        )


@policy_registry.register
class UnitreeWoGaitPolicy(UnitreePolicy):
    cfg_policy: UnitreeWoGaitPolicyCfg

    def __init__(self, cfg_policy, device):
        super().__init__(cfg_policy=cfg_policy, device=device)

    def reset(self):
        self.timestep: int = 0

        history_obs_dims = self.cfg_policy.history_obs_dims
        default_history = [np.zeros(dim, dtype=np.float32) for dim in history_obs_dims.values()]
        self._init_history(default_history)

    def get_observation(self, env_data, ctrl_data):
        commands = self._get_commands(ctrl_data)

        gravity_orientation = get_gravity_orientation(env_data.base_quat)
        obs_current = [
            env_data.base_ang_vel * self.obs_scales.ang_vel,
            gravity_orientation * self.obs_scales.gravity,
            commands * self.obs_scales.command * self.max_cmd,
            (env_data.dof_pos - self.default_dof_pos) * self.obs_scales.dof_pos,
            env_data.dof_vel * self.obs_scales.dof_vel,
            self.last_action,
        ]
        self.history_buf.append(obs_current)

        history_list = [np.concatenate(items, axis=0) for items in zip(*self.history_buf, strict=True)]
        obs = np.concatenate(history_list, axis=0)

        extras = {
            "commands": commands,
        }
        return obs, extras
