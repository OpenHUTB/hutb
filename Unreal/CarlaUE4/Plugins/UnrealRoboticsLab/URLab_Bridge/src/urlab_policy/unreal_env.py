# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
# Licensed under the Apache License, Version 2.0.

"""
UnrealEnv — A RoboJuDo Environment backed by Unreal Robotics Lab over ZMQ.
"""

import json
import logging
import struct
import time

import numpy as np
import zmq

from .env_config import UnrealEnvCfg

logger = logging.getLogger(__name__)

try:
    from robojudo.environment import Environment, env_registry
    HAS_ROBOJUDO = True
except ImportError:
    HAS_ROBOJUDO = False

    class Environment:
        def __init__(self, cfg_env, device="cpu"):
            self.cfg_env = cfg_env
            self.device = device


class ZmqLink:
    """Manages ZMQ sockets for Unreal communication."""

    def __init__(self, state_endpoint: str, control_endpoint: str):
        self.ctx = zmq.Context()

        self.state_sub = self.ctx.socket(zmq.SUB)
        self.state_sub.connect(state_endpoint)
        self.state_sub.setsockopt_string(zmq.SUBSCRIBE, "")
        self.state_sub.setsockopt(zmq.RCVTIMEO, 100)

        self.ctrl_pub = self.ctx.socket(zmq.PUB)
        self.ctrl_pub.connect(control_endpoint)
        time.sleep(0.2)

        logger.info(f"ZMQ state: {state_endpoint}  control: {control_endpoint}")

    def set_prefix_filter(self, prefix: str):
        """Switch from receiving all topics to only those matching prefix."""
        self.state_sub.setsockopt_string(zmq.UNSUBSCRIBE, "")
        self.state_sub.setsockopt_string(zmq.SUBSCRIBE, prefix)
        logger.info(f"ZMQ subscription filtered to prefix: '{prefix}'")

    def drain(self) -> dict[str, bytes]:
        """Drain all pending messages, keep latest per topic."""
        latest = {}
        while True:
            try:
                topic = self.state_sub.recv_string(zmq.NOBLOCK)
                if self.state_sub.getsockopt(zmq.RCVMORE):
                    latest[topic] = self.state_sub.recv()
            except zmq.Again:
                break
        return latest

    def send_control(self, prefix: str, targets: np.ndarray, actuator_ids: list[int] | None = None):
        """Send control values. Format: int32 N, then N x (int32 id, float32 value).
        If actuator_ids is provided, use those IDs instead of sequential 0..N-1."""
        n = len(targets)
        data = struct.pack("<i", n)
        for i, val in enumerate(targets):
            aid = actuator_ids[i] if actuator_ids else i
            data += struct.pack("<if", aid, float(val))
        self.ctrl_pub.send_string(f"{prefix}/control ", zmq.SNDMORE)
        self.ctrl_pub.send(data)

    def send_gains(self, prefix: str, joint_names: list[str],
                    kp: np.ndarray, kv: np.ndarray, torque_limits: np.ndarray):
        """Send PD gains to Unreal's UMjPDController via name-keyed JSON."""
        gains = {}
        for i, name in enumerate(joint_names):
            gains[name] = {
                "kp": float(kp[i]) if i < len(kp) else 100.0,
                "kv": float(kv[i]) if i < len(kv) else 5.0,
                "torque_limit": float(torque_limits[i]) if i < len(torque_limits) else 200.0,
            }
        self.ctrl_pub.send_string(f"{prefix}/set_gains ", zmq.SNDMORE)
        self.ctrl_pub.send_string(json.dumps(gains))

    def close(self):
        self.state_sub.close()
        self.ctrl_pub.close()
        self.ctx.term()


def _detect_prefix_and_joints(zmq_link: ZmqLink, expected_joint_names: list[str],
                              forced_prefix: str = "", timeout: float = 5.0):
    """
    Detect the articulation prefix and build a joint-name-to-ZMQ-ID map
    by listening to incoming topics.

    If forced_prefix is set, only matches joints from that specific articulation.
    Otherwise auto-detects the first prefix seen.

    Returns: (prefix, joint_id_map) where joint_id_map maps joint_name -> zmq_id
    """
    if forced_prefix:
        logger.info(f"Using specified articulation prefix: '{forced_prefix}'")
    else:
        logger.info("Auto-detecting articulation prefix and joint mapping...")

    discovered = {}  # joint_name -> zmq_id
    prefix = forced_prefix or None
    start = time.time()

    while time.time() - start < timeout:
        messages = zmq_link.drain()
        for topic, payload in messages.items():
            if "/joint/" in topic and len(payload) == 16:
                jid, _pos, _vel, _acc = struct.unpack("<Ifff", payload)
                # Topic format: PREFIX/joint/JOINT_NAME
                parts = topic.split("/joint/")
                if len(parts) == 2:
                    pfx, joint_name = parts

                    # If forced prefix, skip joints from other articulations
                    if forced_prefix and pfx != forced_prefix:
                        continue

                    discovered[joint_name] = jid
                    if prefix is None:
                        prefix = pfx
                        logger.info(f"  Detected prefix: '{prefix}'")

        if prefix and len(discovered) >= len(expected_joint_names):
            break
        time.sleep(0.05)

    if prefix is None:
        logger.warning("Could not detect prefix — no joint data received")
        return "", {}

    # Build map: expected joint name -> ZMQ ID
    joint_id_map = {}
    for name in expected_joint_names:
        if name in discovered:
            joint_id_map[name] = discovered[name]
        else:
            logger.warning(f"  Joint '{name}' not found in ZMQ stream")

    logger.info(f"  Mapped {len(joint_id_map)}/{len(expected_joint_names)} joints")

    # Log all discovered joints for debug
    for name, jid in sorted(discovered.items(), key=lambda x: x[1]):
        matched = "OK" if name in joint_id_map else "UNMAPPED"
        logger.info(f"    ZMQ ID {jid:2d}: {name} [{matched}]")

    return prefix, joint_id_map


if HAS_ROBOJUDO:

    @env_registry.register
    class UnrealEnv(Environment):
        """
        RoboJuDo Environment backed by Unreal Robotics Lab over ZMQ.
        Auto-detects articulation prefix and joint mapping at startup.
        """

        cfg_env: UnrealEnvCfg

        def __init__(self, cfg_env: UnrealEnvCfg, device="cpu"):
            super().__init__(cfg_env=cfg_env, device=device)

            # Ensure optional base state fields are never None (base class leaves them as None)
            self._base_pos = np.zeros(3)
            self._base_lin_vel = np.zeros(3)
            self._torso_pos = np.zeros(3)
            self._torso_quat = np.array([0.0, 0.0, 0.0, 1.0])
            self._torso_ang_vel = np.zeros(3)

            self.zmq = ZmqLink(cfg_env.state_endpoint, cfg_env.control_endpoint)
            self.control_dt = cfg_env.sim_dt * cfg_env.sim_decimation

            self._connected = False
            self._last_update = 0.0
            self._base_state_received = False
            self._twist_cmd = np.zeros(3)  # [vx, vy, yaw_rate] from Unreal input
            self._force_twist = None  # Set externally to override ZMQ twist

            # Detect prefix and joint mapping (uses forced prefix if set in config)
            forced = getattr(cfg_env, 'articulation_prefix', '') or ''
            self.prefix, self._joint_id_map = _detect_prefix_and_joints(
                self.zmq, self.joint_names, forced_prefix=forced
            )

            # Build reverse map: zmq_id -> dof_index
            self._rebuild_zmq_mapping()

            # Discover actuator IDs from the info endpoint
            self._cfg_env = cfg_env
            self._actuator_ids = self._discover_actuator_ids(cfg_env)

            # Filter ZMQ subscription to only our articulation's topics
            self.zmq.set_prefix_filter(self.prefix)

            logger.info(f"UnrealEnv ready — prefix='{self.prefix}', {self.num_dofs} DOFs, "
                        f"control_dt={self.control_dt:.4f}s ({1.0/self.control_dt:.0f}Hz)")

        def _rebuild_zmq_mapping(self):
            """Rebuild zmq_id -> dof_index and actuator ID mappings from current joint_names."""
            self._zmq_id_to_dof = {}
            for dof_idx, name in enumerate(self.joint_names):
                if name in self._joint_id_map:
                    self._zmq_id_to_dof[self._joint_id_map[name]] = dof_idx

        def update_dof_cfg(self, override_cfg=None):
            """Override to rebuild ZMQ mappings when the pipeline reorders DOFs."""
            super().update_dof_cfg(override_cfg)
            if hasattr(self, '_joint_id_map'):
                self._rebuild_zmq_mapping()
                # Rebuild actuator IDs for the new joint order
                if hasattr(self, '_cfg_env'):
                    self._actuator_ids = self._discover_actuator_ids(self._cfg_env)
                logger.info(f"DOF config updated — {self.num_dofs} DOFs, joints: {self.joint_names[:3]}...")

        def _discover_actuator_ids(self, cfg_env) -> list[int] | None:
            """Listen to the info endpoint to discover actuator name→MjID mapping."""
            info_endpoint = getattr(cfg_env, "info_endpoint", "tcp://127.0.0.1:5557")
            ctx = zmq.Context()
            info_sub = ctx.socket(zmq.SUB)
            info_sub.connect(info_endpoint)
            info_sub.setsockopt_string(zmq.SUBSCRIBE, "")
            info_sub.setsockopt(zmq.RCVTIMEO, 5000)

            actuator_ids = None
            try:
                for _ in range(20):
                    try:
                        payload = info_sub.recv().decode("utf-8")
                        data = json.loads(payload)
                        if data.get("type") == "actuator_list" and data.get("robot") == self.prefix:
                            names = data.get("names", [])
                            ids = data.get("ids", [])
                            # Build DOF-ordered actuator ID list
                            name_to_id = {}
                            for n, i in zip(names, ids):
                                # Strip the prefix to get the original joint/actuator name
                                short = n.replace(self.prefix + "_", "", 1)
                                name_to_id[short] = int(i)

                            actuator_ids = []
                            for jname in self.joint_names:
                                if jname in name_to_id:
                                    actuator_ids.append(name_to_id[jname])
                                elif jname.removesuffix("_joint") in name_to_id:
                                    # Menagerie XMLs name actuators without _joint suffix
                                    actuator_ids.append(name_to_id[jname.removesuffix("_joint")])
                                else:
                                    logger.warning(f"  Actuator for joint '{jname}' not found in info")
                                    actuator_ids.append(-1)

                            logger.info(f"  Discovered actuator IDs: {actuator_ids}")
                            break
                    except zmq.Again:
                        break
            finally:
                info_sub.close()
                ctx.term()

            return actuator_ids

        def self_check(self):
            logger.info("Running self-check...")
            for i in range(50):
                self.update()
                if self._connected:
                    logger.info(f"Self-check passed — data flowing after {i * 100}ms")
                    return
                time.sleep(0.1)
            logger.warning("Self-check: no data in 5s")

        def reset(self):
            self._dof_pos = np.zeros(self.num_dofs)
            self._dof_vel = np.zeros(self.num_dofs)
            self._base_quat = np.array([0.0, 0.0, 0.0, 1.0])
            self._base_ang_vel = np.zeros(3)
            self._base_pos = np.zeros(3)
            self._base_lin_vel = np.zeros(3)

            if self.born_place_align:
                self.update()
                self.set_born_place()
                self.update()

        def set_born_place(self, quat=None, pos=None):
            super().set_born_place(
                quat if quat is not None else self.base_quat,
                pos if pos is not None else self.base_pos,
            )

        def set_gains(self, stiffness, damping):
            self.stiffness = np.asarray(stiffness)
            self.damping = np.asarray(damping)

        def update(self, simple=False):
            """Pull latest state from ZMQ, map to DOF indices."""
            messages = self.zmq.drain()

            for topic, payload in messages.items():
                # Filter by prefix — only process messages from our articulation
                if not topic.startswith(self.prefix):
                    continue

                if "/joint/" in topic and len(payload) == 16:
                    zmq_id, pos, vel, _acc = struct.unpack("<Ifff", payload)
                    if zmq_id in self._zmq_id_to_dof:
                        dof_idx = self._zmq_id_to_dof[zmq_id]
                        self._dof_pos[dof_idx] = pos
                        self._dof_vel[dof_idx] = vel
                        self._connected = True

                elif "/base_state/" in topic and len(payload) == 52:
                    # Free joint full state: pos[3], quat[4] (x,y,z,w), linvel[3], angvel[3]
                    vals = struct.unpack("<13f", payload)
                    quat = np.array(vals[3:7])  # already x,y,z,w from C++
                    pos = np.array(vals[0:3])
                    lin_vel = np.array(vals[7:10])
                    ang_vel = np.array(vals[10:13])

                    # Apply born place alignment (matches mujoco_env.py)
                    if self.born_place_align:
                        quat, pos = self.base_align.align_transform(quat, pos)

                    self._base_pos = pos
                    self._base_quat = quat
                    self._base_lin_vel = lin_vel
                    self._base_ang_vel = ang_vel
                    self._connected = True
                    if not self._base_state_received:
                        self._base_state_received = True
                        logger.info(f"Base state received! pos={self._base_pos}, "
                                    f"quat={self._base_quat}, angvel={self._base_ang_vel}")

                elif "/twist" in topic and len(payload) == 12:
                    self._twist_cmd = np.array(struct.unpack("<3f", payload))

                elif not simple and "/sensor/" in topic and len(payload) >= 8:
                    sid, dim = struct.unpack("<II", payload[:8])
                    expected = 8 + dim * 4
                    if len(payload) == expected:
                        vals = np.array(struct.unpack(f"<{dim}f", payload[8:]))

                        # Torso-specific sensors (framepos/framequat on imu_in_torso)
                        if "torso-framepos" in topic and dim == 3:
                            self._torso_pos = vals.copy()
                        elif "torso-framequat" in topic and dim == 4:
                            # MuJoCo framequat outputs (w,x,y,z), convert to (x,y,z,w)
                            self._torso_quat = np.array([vals[1], vals[2], vals[3], vals[0]])
                        # Pelvis-specific sensors
                        elif "pelvis-framepos" in topic and dim == 3:
                            self._base_pos = vals.copy()
                        elif "pelvis-framequat" in topic and dim == 4:
                            self._base_quat = np.array([vals[1], vals[2], vals[3], vals[0]])
                        # IMU gyro → angular velocity
                        elif "gyro" in topic.lower() and dim == 3:
                            if "torso" in topic.lower():
                                self._torso_ang_vel = vals.copy()
                            self._base_ang_vel = vals.copy()
                        elif "accel" in topic.lower() and dim == 3:
                            self._base_lin_acc = vals.copy()

            self._last_update = time.time()

            # Run forward kinematics to compute torso position (matches mujoco_env.py)
            if not simple and self.update_with_fk and self.kinematics is not None and self._connected:
                try:
                    fk_info = self.fk()
                    self._fk_info = fk_info.copy()
                    if self._torso_name in fk_info:
                        self._torso_pos = fk_info[self._torso_name]["pos"]
                        self._torso_quat = fk_info[self._torso_name]["quat"]
                        self._torso_ang_vel = fk_info[self._torso_name]["ang_vel"]
                except Exception:
                    pass  # FK may fail before enough state is received

        def step(self, pd_target, hand_pose=None):
            assert len(pd_target) == self.num_dofs, (
                f"pd_target len {len(pd_target)} != num_dofs {self.num_dofs}"
            )
            # Send position targets directly — MuJoCo position actuators handle PD
            # internally at native timestep rate (avoids stale-torque oscillation)
            self.zmq.send_control(self.prefix, pd_target, self._actuator_ids)

        @property
        def twist_cmd(self) -> np.ndarray:
            """Current twist command [vx, vy, yaw_rate]. Returns forced twist if set, else ZMQ twist."""
            if self._force_twist is not None:
                return self._force_twist
            return self._twist_cmd

        def shutdown(self):
            self.zmq.close()
            logger.info("UnrealEnv shut down")


class UnrealEnvStandalone:
    """Minimal ZMQ bridge for testing without RoboJuDo."""

    def __init__(self, cfg_env: UnrealEnvCfg):
        self.cfg = cfg_env
        self.zmq = ZmqLink(cfg_env.state_endpoint, cfg_env.control_endpoint)
        self.prefix = cfg_env.articulation_prefix
        self.num_dofs = len(getattr(cfg_env, "joint_names", [])) or 12

        self._dof_pos = np.zeros(self.num_dofs)
        self._dof_vel = np.zeros(self.num_dofs)
        self._connected = False

    @property
    def connected(self):
        return self._connected

    def update(self):
        for topic, payload in self.zmq.drain().items():
            if "/joint/" in topic and len(payload) == 16:
                jid, pos, vel, _acc = struct.unpack("<Ifff", payload)
                if 0 <= jid < self.num_dofs:
                    self._dof_pos[jid] = pos
                    self._dof_vel[jid] = vel
                    self._connected = True

    def step(self, pd_target):
        self.zmq.send_control(self.prefix, pd_target)

    def shutdown(self):
        self.zmq.close()
