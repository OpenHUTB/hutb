# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
# Licensed under the Apache License, Version 2.0.

"""
Walk-These-Ways policy for quadruped locomotion (Unitree Go2).

Uses two TorchScript models:
- body: main policy network (obs_history + latent -> actions)
- adaptation_module: processes obs history into a latent vector

Observation per step (70 dims):
  gravity(3) + commands(15) + dof_pos(12) + dof_vel(12) + actions(12)
  + prev_actions(12) + clock_inputs(4)

History: 30 steps -> 2100 total input to adaptation module
Body input: 2100 + 2 (latent) = 2102

Reference: https://github.com/Teddy-Liao/walk-these-ways-go2
"""

import logging
from collections import deque

import numpy as np
import torch

from robojudo.policy import Policy, policy_registry
from robojudo.policy.policy_cfgs import WalkTheseWaysPolicyCfg
from robojudo.utils.util_func import command_remap, get_gravity_orientation

logger = logging.getLogger(__name__)


@policy_registry.register
class WalkTheseWaysPolicy(Policy):
    """Gait-conditioned agility policy for quadruped robots."""

    cfg_policy: WalkTheseWaysPolicyCfg

    def __init__(self, cfg_policy, device):
        super().__init__(cfg_policy=cfg_policy, device=device)

        self.obs_scales = self.cfg_policy.obs_scales
        self.max_cmd = self.cfg_policy.max_cmd
        self.commands_map = self.cfg_policy.commands_map
        self.hip_scale_reduction = self.cfg_policy.hip_scale_reduction
        self.num_obs_per_step = self.cfg_policy.num_obs_per_step
        self.default_gait_commands = np.array(self.cfg_policy.default_gait_commands, dtype=np.float32)

        # Commands scale matching WTW training obs_scales (15 dims)
        self.commands_scale = np.array([
            2.0,    # lin_vel (x)
            2.0,    # lin_vel (y)
            0.25,   # ang_vel (yaw)
            2.0,    # body_height_cmd
            1.0,    # gait_freq_cmd
            1.0,    # gait_phase_cmd
            1.0,    # gait_phase_cmd (offset)
            1.0,    # gait_phase_cmd (bound)
            1.0,    # gait_phase_cmd (duration)
            0.15,   # footswing_height_cmd
            0.3,    # body_pitch_cmd
            0.3,    # body_roll_cmd
            1.0,    # stance_width_cmd
            1.0,    # stance_length_cmd
            1.0,    # aux_reward_cmd
        ], dtype=np.float32)

        # Load the two TorchScript models
        logger.info(f"Loading WTW body from {self.cfg_policy.policy_file}")
        self.body = torch.jit.load(self.cfg_policy.policy_file, map_location=self.device)
        self.body.eval()

        logger.info(f"Loading WTW adaptation module from {self.cfg_policy.adaptation_module_file}")
        self.adaptation_module = torch.jit.load(
            self.cfg_policy.adaptation_module_file, map_location=self.device
        )
        self.adaptation_module.eval()

        # History buffer: stores flattened obs per timestep
        self.obs_history_buf = deque(maxlen=self.cfg_policy.history_length)

        # Previous actions buffer (for observe_two_prev_actions)
        self.prev_actions = np.zeros(self.num_actions, dtype=np.float32)

        # Clock inputs (4 dims): sin/cos for two gait phases
        self.gait_phase = np.array([0.0, 0.5], dtype=np.float32)  # two legs offset by 0.5
        self._is_standing = True
        self._was_walking = False

        self.reset()

    def reset(self):
        self.timestep: int = 0
        self.obs_history_buf.clear()
        for _ in range(self.cfg_policy.history_length):
            self.obs_history_buf.append(np.zeros(self.num_obs_per_step, dtype=np.float32))
        self.prev_actions = np.zeros(self.num_actions, dtype=np.float32)
        self.gait_phase = np.array([0.0, 0.5], dtype=np.float32)
        self._is_standing = True
        self._smoothed_twist = np.zeros(3, dtype=np.float32)
        self._was_walking = False

    def set_gait_preset(self, preset_name: str):
        """Switch gait mode by name (e.g. 'Trot', 'Pronk', 'Bound')."""
        presets = self.cfg_policy.GAIT_PRESETS
        if preset_name not in presets:
            logger.warning(f"Unknown gait preset '{preset_name}', available: {list(presets.keys())}")
            return
        self.default_gait_commands = np.array(presets[preset_name], dtype=np.float32)
        # Update gait clock frequency from the new preset
        self.gait_phase = np.array([0.0, 0.5], dtype=np.float32)
        logger.info(f"Gait preset set to '{preset_name}': {self.default_gait_commands}")

    def post_step_callback(self, commands=None):
        self.timestep += 1

        # Always advance gait clock — the policy was trained with a running clock
        gait_freq = self.default_gait_commands[1]  # index 1 = gait_frequency
        self.gait_phase = np.fmod(self.gait_phase + self.dt * gait_freq, 1.0)

    def _get_commands(self, ctrl_data):
        """Get 15-dim command vector: [vx, vy, yaw, gait_params...]."""
        twist = np.zeros(3)
        for key in ctrl_data.keys():
            if key in ["JoystickCtrl", "UnitreeCtrl"]:
                axes = ctrl_data[key]["axes"]
                lx, ly, rx = axes["LeftX"], axes["LeftY"], axes["RightX"]
                twist[0] = command_remap(ly, self.commands_map[0])
                twist[1] = command_remap(lx, self.commands_map[1])
                twist[2] = command_remap(rx, self.commands_map[2])
                break
            if key in ["KeyboardCtrl"]:
                keys = ctrl_data[key]["keyboard_event"]
                for event in keys:
                    if event["type"] == "keyboard":
                        value = event["pressed"] * 1.5
                        match event["name"]:
                            case "w":
                                twist[0] = command_remap(value, self.commands_map[0])
                            case "s":
                                twist[0] = command_remap(-value, self.commands_map[0])
                            case "a":
                                twist[1] = command_remap(-value, self.commands_map[1])
                            case "d":
                                twist[1] = command_remap(value, self.commands_map[1])
                            case "e":
                                twist[2] = command_remap(value, self.commands_map[2])
                            case "q":
                                twist[2] = command_remap(-value, self.commands_map[2])
                break

        # Scale twist by max_cmd to get velocities in m/s and rad/s
        twist[0] *= self.max_cmd[0]  # vx
        twist[1] *= self.max_cmd[1]  # vy
        twist[2] *= self.max_cmd[2]  # yaw

        # Smooth ramp — prevents scrambling when going from 0 to full speed
        ramp_rate = 0.02  # per step, ~1 second from 0 to full at 50Hz
        self._smoothed_twist = self._smoothed_twist + np.clip(
            twist - self._smoothed_twist, -ramp_rate, ramp_rate
        )
        twist = self._smoothed_twist.copy()

        # Update standing flag
        self._is_standing = np.all(np.abs(twist) < 0.1)

        # Full 15-dim command: [vx, vy, yaw, gait_params(12)]
        commands = np.concatenate([twist, self.default_gait_commands])
        return commands

    def _get_clock_inputs(self):
        """4-dim clock signal: sin/cos for two gait phases."""
        return np.array([
            np.sin(2 * np.pi * self.gait_phase[0]),
            np.cos(2 * np.pi * self.gait_phase[0]),
            np.sin(2 * np.pi * self.gait_phase[1]),
            np.cos(2 * np.pi * self.gait_phase[1]),
        ], dtype=np.float32)

    def get_observation(self, env_data, ctrl_data):
        commands = self._get_commands(ctrl_data)
        clock_inputs = self._get_clock_inputs()
        gravity_orientation = get_gravity_orientation(env_data.base_quat)

        # Build 70-dim observation matching WTW training order:
        # gravity(3) + commands*scale(15) + dof_pos*scale(12) + dof_vel*scale(12)
        # + actions(12) + prev_actions(12) + clock_inputs(4)
        obs_step = np.concatenate([
            gravity_orientation,                                    # 3
            commands * self.commands_scale,                         # 15
            (env_data.dof_pos - self.default_dof_pos) * self.obs_scales.dof_pos,  # 12
            env_data.dof_vel * self.obs_scales.dof_vel,            # 12
            self.last_action,                                       # 12
            self.prev_actions,                                      # 12
            clock_inputs,                                           # 4
        ]).astype(np.float32)                                       # = 70

        # Debug: log obs stats every 50 steps
        if self.timestep % 50 == 0 and self.timestep <= 500:
            logger.info(
                f"[WTW obs] step={self.timestep} "
                f"gravity={np.round(gravity_orientation, 3)} "
                f"dof_err={np.round(env_data.dof_pos - self.default_dof_pos, 2)} "
                f"cmd={np.round(commands[:3], 3)} "
                f"standing={self._is_standing} "
                f"clock={np.round(self.gait_phase, 2)} "
                f"action={np.round(self.last_action, 2)}"
            )

        # Update prev_actions for next step
        self.prev_actions = self.last_action.copy()

        # Append to history
        self.obs_history_buf.append(obs_step)

        # Flatten history: oldest first -> newest last (30 * 70 = 2100)
        obs = np.concatenate(list(self.obs_history_buf))

        extras = {
            "commands": commands[:3],  # just twist for debug viz
        }
        return obs, extras

    def get_action(self, obs: np.ndarray) -> np.ndarray:
        # When standing still, hold default pose instead of running the policy.
        # The policy wasn't trained to stand and produces jittery output at zero command.
        if self._is_standing:
            self.last_action = self.last_action * 0.9  # decay smoothly to zero
            return np.zeros(self.num_actions)

        # Guard against NaN in observation (can happen if robot starts far from default)
        if np.any(np.isnan(obs)) or np.any(np.isinf(obs)):
            logger.warning("[WTW] NaN/Inf in observation, returning zero action")
            self.last_action = np.zeros(self.num_actions)
            return np.zeros(self.num_actions)

        obs_history_tensor = torch.from_numpy(obs).unsqueeze(0).float().to(self.device)

        with torch.no_grad():
            # Adaptation module: obs_history(2100) -> latent(2)
            latent = self.adaptation_module(obs_history_tensor)

            # Body: cat(obs_history(2100), latent(2)) -> actions(12)
            body_input = torch.cat((obs_history_tensor, latent), dim=-1)
            actions_tensor = self.body(body_input)

        actions = actions_tensor.cpu().numpy().squeeze()

        # Guard against NaN output from network
        if np.any(np.isnan(actions)):
            logger.warning("[WTW] NaN in network output, returning zero action")
            self.last_action = np.zeros(self.num_actions)
            return np.zeros(self.num_actions)

        # Clip raw actions to prevent runaway (WTW training uses clip_actions=10.0)
        actions = np.clip(actions, -10.0, 10.0)

        # Action smoothing
        actions = (1 - self.action_beta) * self.last_action + self.action_beta * actions
        self.last_action = actions.copy()

        # Scale actions — reduce hip joints
        scaled = actions * self.action_scale
        # Hip joints are at indices 0, 3, 6, 9 (first joint of each leg)
        scaled[0::3] *= self.hip_scale_reduction

        return scaled
