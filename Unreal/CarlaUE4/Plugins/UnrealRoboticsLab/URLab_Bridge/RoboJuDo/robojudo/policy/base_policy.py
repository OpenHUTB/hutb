import logging
from abc import ABC, abstractmethod
from collections import deque

import numpy as np
import torch

from robojudo.tools.tool_cfgs import DoFConfig

from .policy_cfgs import PolicyCfg

logger = logging.getLogger(__name__)


class Policy(ABC):
    def __init__(self, cfg_policy: PolicyCfg, device: str = "cpu"):
        self.cfg_policy = cfg_policy
        self.device = device

        self.freq = self.cfg_policy.freq
        self.dt = 1.0 / self.freq

        self.cfg_obs_dof: DoFConfig = self.cfg_policy.obs_dof
        self.cfg_action_dof: DoFConfig = self.cfg_policy.action_dof

        self.num_dofs = self.cfg_obs_dof.num_dofs
        self.num_actions = self.cfg_action_dof.num_dofs

        self.default_dof_pos = np.asarray(self.cfg_obs_dof.default_pos)
        self.default_pos = np.asarray(self.cfg_action_dof.default_pos)  # TODO: remove

        # TODO: autoload cfg
        if self.cfg_policy.disable_autoload:
            # self.model: torch.nn.Module | None = None # type: ignore
            pass
        else:
            policy_file = self.cfg_policy.policy_file
            logger.debug(f"Loading jit from {policy_file}...")
            self.model = torch.jit.load(policy_file, map_location=self.device)

        self.action_scale = self.cfg_policy.action_scale
        self.action_clip = self.cfg_policy.action_clip
        self.action_beta = self.cfg_policy.action_beta

        self.last_action = np.zeros(self.num_actions)

        self.history_length = self.cfg_policy.history_length
        self.history_obs_size = self.cfg_policy.history_obs_size

    def _init_history(self, default_history: np.ndarray | torch.Tensor | list):
        logger.debug(f"Initializing history buffer as {self.history_length} x {len(default_history)}")
        self.history_buf = deque(maxlen=self.history_length)
        for _ in range(self.history_length):
            self.history_buf.append(default_history)

    @abstractmethod
    def reset(self):
        # self.last_action = np.zeros(self.num_actions) # TODO
        raise NotImplementedError

    def reset_alignment(self):
        """Reset heading/spatial alignment without resetting motion playback state.

        Override in policies that compute a heading offset at runtime (e.g. ProtoMotions
        trackers). The default no-op is correct for policies that don't have alignment state.
        """

    @abstractmethod
    def post_step_callback(self, commands: list[str] | None = None):
        raise NotImplementedError

    @abstractmethod
    def get_observation(self, env_data, ctrl_data) -> tuple[np.ndarray, dict]:
        raise NotImplementedError

    def get_action(self, obs: np.ndarray) -> np.ndarray:
        obs_tensor = torch.from_numpy(obs).unsqueeze(0).float().to(self.device)
        with torch.no_grad():
            actions_tensor = self.model(obs_tensor).cpu()

        actions = actions_tensor.numpy().squeeze()
        actions = (1 - self.action_beta) * self.last_action + self.action_beta * actions

        self.last_action = actions.copy()  # TODO

        processed_actions = actions
        if self.action_clip is not None:
            processed_actions = np.clip(processed_actions, -self.action_clip, self.action_clip)

        processed_actions = processed_actions * self.action_scale
        return processed_actions

    def get_init_dof_pos(self) -> np.ndarray:
        """
        Return the initial dof pos for the policy, used for robot preparation.
        For motion policies, this should return next/first frame of the reference motion.
        """
        return self.default_pos.copy()

    def debug_viz(self, visualizer, env_data, ctrl_data, extras):
        # for debug draw
        return
