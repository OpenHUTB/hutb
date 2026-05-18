import logging

import robojudo.environment
import robojudo.policy
from robojudo.controller import CtrlManager
from robojudo.environment import Environment
from robojudo.pipeline import Pipeline, pipeline_registry
from robojudo.pipeline.pipeline_cfgs import RlMultiPolicyPipelineCfg
from robojudo.pipeline.rl_pipeline import PolicyWrapper, RlPipeline
from robojudo.policy import PolicyCfg
from robojudo.utils.step_timer import StepTimer

logger = logging.getLogger(__name__)


class PolicyManager:
    DELAY_STEPS_SWITCH: int = 10  # steps to wait before switching policy

    def __init__(
        self,
        cfg_policies: list[PolicyCfg],
        env: Environment,
        device: str = "cpu",
    ):
        self.env = env
        self.device = device

        self.policies: list[PolicyWrapper] = []
        for cfg_policy in cfg_policies:
            policy_entry = PolicyWrapper(cfg_policy, self.env.dof_cfg, device)
            self.policies.append(policy_entry)

        self._current_policy_id: int = 0
        self.warmup_policy_indices = set()

        self.timer = StepTimer()

    @property
    def current_policy_id(self):
        return self._current_policy_id

    @property
    def num_policies(self):
        return len(self.policies)

    @property
    def policy(self) -> PolicyWrapper:
        return self.policies[self.current_policy_id]

    def policy_by_id(self, policy_id) -> PolicyWrapper:
        if not (0 <= policy_id < self.num_policies):
            raise ValueError(f"Policy id {policy_id} out of range [0, {self.num_policies})")
        return self.policies[policy_id]

    def set_policy(self, policy_id: int):
        """Instantly set the policy as policy_id."""
        if not (0 <= policy_id < self.num_policies):
            raise ValueError(f"Policy id {policy_id} out of range [0, {self.num_policies})")
        self.warmup_policy_indices.discard(policy_id)

        self._current_policy_id = policy_id
        # refresh env
        self.env.reset()
        self.env.update_dof_cfg(override_cfg=self.policy.cfg_action_dof)
        logger.warning(f"Switched to policy: {policy_id}: {self.policy.name}")

    def switch_policy(self, policy_id: int):
        """Switch to the policy as policy_id after delay."""
        if not (0 <= policy_id < self.num_policies):
            raise ValueError(f"Policy id {policy_id} out of range [0, {self.num_policies})")
        self.policy_by_id(policy_id).reset()
        self.warmup_policy_indices.add(policy_id)
        self.timer.add(lambda: self.set_policy(policy_id), delay_steps=self.DELAY_STEPS_SWITCH)

    def step(self, env_data, ctrl_data):
        # policy warmup
        for idx in self.warmup_policy_indices:
            if idx != self.current_policy_id:
                self.policy_by_id(idx).get_observation(env_data, ctrl_data)

        self.timer.tick()


@pipeline_registry.register
class RlMultiPolicyPipeline(RlPipeline):
    cfg: RlMultiPolicyPipelineCfg

    @property
    def policy(self) -> PolicyWrapper:
        return self.policy_manager.policy

    def __init__(self, cfg: RlMultiPolicyPipelineCfg):
        # Skip RlPipeline initialization
        Pipeline.__init__(self, cfg=cfg)

        env_class: type[Environment] = getattr(robojudo.environment, self.cfg.env.env_type)
        self.env: Environment = env_class(cfg_env=self.cfg.env, device=self.device)

        self.ctrl_manager = CtrlManager(cfg_ctrls=self.cfg.ctrl, env=self.env, device=self.device)

        self.policy_manager = PolicyManager(
            cfg_policies=self.cfg.policies,
            env=self.env,
            device=self.device,
        )
        self.env.update_dof_cfg(override_cfg=self.policy.cfg_action_dof)
        self.visualizer = self.env.visualizer

        self.freq = self.cfg.policies[0].freq
        self.dt = 1.0 / self.freq

        self.self_check()
        self.reset()

    def self_check(self):
        self.policy_manager.warmup_policy_indices = set(list(range(self.policy_manager.num_policies)))
        super().self_check()
        self.policy_manager.warmup_policy_indices = set()

    def post_step_callback(self, env_data, ctrl_data, extras, pd_target):
        self.timestep += 1

        commands = ctrl_data.get("COMMANDS", [])
        for command in commands:
            match command:
                case "[SHUTDOWN]":
                    logger.warning("Emergency shutdown!")
                    self.env.shutdown()
                case "[SIM_REBORN]":
                    if hasattr(self.env, "reborn"):
                        logger.warning("Simulation Env reborn!")
                        self.env.reborn()  # pyright: ignore[reportAttributeAccessIssue]
                case "[POLICY_TOGGLE]":
                    logger.warning("Policy toggled!")
                    next_policy_id = (self.policy_manager.current_policy_id + 1) % self.policy_manager.num_policies
                    self.policy_manager.switch_policy(next_policy_id)

                case cmd if cmd.startswith("[POLICY_SWITCH]"):
                    policy_id = int(cmd.split(",")[1])
                    if policy_id < self.policy_manager.num_policies:
                        self.policy_manager.switch_policy(policy_id)

        self.ctrl_manager.post_step_callback(ctrl_data)

        self.policy.post_step_callback(commands)
        if self.visualizer is not None:
            self.policy.debug_viz(self.visualizer, env_data, ctrl_data, extras)

        self.policy_manager.step(env_data, ctrl_data)

        self.safety_check()
        if self.cfg.debug.log_obs:
            self.debug_logger.log(
                env_data=env_data,
                ctrl_data=ctrl_data,
                extras=extras,
                pd_target=pd_target,
                timestep=self.timestep,
            )

    def step(self, dry_run=False):
        self.env.update()
        env_data = self.env.get_data()
        ctrl_data = self.ctrl_manager.get_ctrl_data(env_data)

        commands = ctrl_data.get("COMMANDS", [])
        if len(commands) > 0:
            logger.info(f"{'=' * 10} COMMANDS {'=' * 10}\n{commands}")

        obs, extras = self.policy.get_observation(env_data, ctrl_data)

        pd_target = self.policy.get_pd_target(obs)

        if not dry_run:
            self.env.step(pd_target, extras.get("hand_pose", None))
            # logger.debug(pd_target)

        self.post_step_callback(env_data, ctrl_data, extras, pd_target)


if __name__ == "__main__":
    pass
