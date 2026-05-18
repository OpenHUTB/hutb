import time

from robojudo.config.g1.ctrl.g1_beyondmimic_ctrl_cfg import G1BeyondmimicCtrlCfg
from robojudo.config.g1.env.g1_dummy_env_cfg import G1DummyEnvCfg
from robojudo.config.g1.policy.g1_beyondmimic_policy_cfg import G1BeyondMimicDoF
from robojudo.controller.beyondmimic_ctrl import BeyondMimicCtrl
from robojudo.environment.dummy_env import DummyEnv
from robojudo.tools.tool_cfgs import ForwardKinematicCfg

bm_dof = G1BeyondMimicDoF()

fk_cfg = ForwardKinematicCfg(
    xml_path=G1DummyEnvCfg.model_fields["xml"].default,
    debug_viz=True,
    kinematic_joint_names=bm_dof.joint_names,
)
env_cfg = G1DummyEnvCfg(forward_kinematic=fk_cfg, odometry_type="DUMMY")

env = DummyEnv(cfg_env=env_cfg)

ctrl_cfg = G1BeyondmimicCtrlCfg(
    motion_name="Box",
)
ctrl_cfg.motion_cfg.anchor_body_name = "pelvis"

ctrl = BeyondMimicCtrl(
    cfg_ctrl=ctrl_cfg,
    env=env,
)
ctrl.playing = True


for i in range(1000):
    joint_pos = ctrl.joint_pos
    base_pos = ctrl.anchor_pos_w
    base_quat = ctrl.anchor_quat_w
    env.kinematics.forward(
        joint_pos=joint_pos,
        base_pos=base_pos,
        base_quat=base_quat,
    )
    ctrl.post_step_callback()
    print(f"Step {i}")
    time.sleep(0.02)
