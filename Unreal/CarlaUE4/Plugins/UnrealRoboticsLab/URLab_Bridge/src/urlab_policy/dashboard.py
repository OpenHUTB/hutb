# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
# Licensed under the Apache License, Version 2.0.

"""
Combined debug GUI: proven ZMQ visualizer + policy runner.
ZMQ code copied directly from zmq_visualizer_gui.py (known working).
Policy runs in a separate thread using its own UnrealEnv ZMQ connection.
"""

import zmq
import struct
import numpy as np
import argparse
import dearpygui.dearpygui as dpg
import time
import threading
import json
import queue
import logging

logger = logging.getLogger(__name__)

# ─── Shared State (from zmq_visualizer_gui.py) ───

robot_states = {}
connected_cam_endpoints = set()
selected_robot = None
selected_camera = None
state_lock = threading.Lock()
control_queue = queue.Queue()

# Policy state
policy_state = {
    "running": False,
    "name": "",
    "step_count": 0,
    "freq_hz": 0.0,
    "last_action": None,
    "last_pd_target": None,
    "error": "",
    "joint_mapping": {},
    "phase": "idle",
    "pending_gait_preset": None,
}
policy_lock = threading.Lock()


def get_robot_state_locked(prefix):
    if prefix not in robot_states:
        logger.debug(f"New Articulation detected: {prefix}")
        robot_states[prefix] = {
            'joints': {},
            'sensors': {},
            'images': {},
            'actuator_config': None,
            'twist': (0.0, 0.0, 0.0),
            'base_state': None,
            'actions': 0,
            'hz': {'overall': 0, 'msg_count': 0, 'start_time': time.time()}
        }
    return robot_states[prefix]


# ─── ZMQ Worker (copied from zmq_visualizer_gui.py — proven working) ───

def zmq_worker_thread(args):
    global connected_cam_endpoints
    context = zmq.Context()

    sub_socket = context.socket(zmq.SUB)
    sub_socket.connect(args.main_endpoint)
    sub_socket.setsockopt_string(zmq.SUBSCRIBE, "")

    cam_socket = context.socket(zmq.SUB)
    cam_socket.connect(args.camera_endpoint)
    connected_cam_endpoints.add(args.camera_endpoint)
    cam_socket.setsockopt_string(zmq.SUBSCRIBE, "")

    info_socket = context.socket(zmq.SUB)
    info_socket.connect(args.info_endpoint)
    info_socket.setsockopt_string(zmq.SUBSCRIBE, "")

    pub_socket = context.socket(zmq.PUB)
    pub_socket.connect(args.control_endpoint)

    poller = zmq.Poller()
    poller.register(sub_socket, zmq.POLLIN)
    poller.register(cam_socket, zmq.POLLIN)
    poller.register(info_socket, zmq.POLLIN)

    logger.info("ZMQ Thread running.")
    logger.info(f"State: {args.main_endpoint}, Cam: {args.camera_endpoint}, Info: {args.info_endpoint}, Control: {args.control_endpoint}")

    while True:
        try:
            while not control_queue.empty():
                robot_name, actuator_id, val = control_queue.get()
                topic = f"{robot_name}/control ".encode('utf-8')
                payload = struct.pack('<iif', 1, actuator_id, val)
                pub_socket.send(topic, zmq.SNDMORE)
                pub_socket.send(payload)

            socks = dict(poller.poll(10))

            with state_lock:
                if info_socket in socks:
                    try:
                        for _ in range(10):
                            payload = info_socket.recv(zmq.NOBLOCK).decode('utf-8')
                            data = json.loads(payload)
                            if data.get('type') == 'actuator_list':
                                robot = data.get('robot')
                                state = get_robot_state_locked(robot)
                                state['actuator_config'] = data
                            if 'camera_list' in data:
                                for entry in data['camera_list']:
                                    ep = entry.get('endpoint')
                                    if ep and ep not in connected_cam_endpoints:
                                        logger.debug(f"Found new Camera Endpoint: {ep}")
                                        cam_socket.connect(ep)
                                        connected_cam_endpoints.add(ep)
                    except zmq.Again:
                        pass

                if sub_socket in socks:
                    try:
                        for _ in range(100):
                            topic = sub_socket.recv(zmq.NOBLOCK).decode('utf-8').strip()
                            if not sub_socket.getsockopt(zmq.RCVMORE):
                                continue
                            payload = sub_socket.recv()
                            parts = topic.split('/', 1)
                            if len(parts) >= 2:
                                prefix = parts[0]
                                subtopic = parts[1]
                                state = get_robot_state_locked(prefix)
                                state['hz']['msg_count'] += 1
                                if 'joint/' in subtopic:
                                    if len(payload) == 16:
                                        jid, p, v, a = struct.unpack('<Ifff', payload)
                                        jname = subtopic.replace('joint/', '').strip()
                                        state['joints'][jname] = (jid, p, v, a)
                                elif 'twist' == subtopic and len(payload) == 12:
                                    state['twist'] = struct.unpack('<3f', payload)
                                elif 'actions' == subtopic and len(payload) == 4:
                                    state['actions'] = struct.unpack('<i', payload)[0]
                                elif 'base_state/' in subtopic and len(payload) == 52:
                                    vals = struct.unpack('<13f', payload)
                                    state['base_state'] = {
                                        'pos': vals[0:3],
                                        'quat': vals[3:7],
                                        'linvel': vals[7:10],
                                        'angvel': vals[10:13],
                                    }
                                elif 'sensor/' in subtopic:
                                    if len(payload) >= 8:
                                        sid, dim = struct.unpack('<II', payload[:8])
                                        if len(payload) == 8 + dim * 4:
                                            vals = struct.unpack(f'<{dim}f', payload[8:])
                                            sname = subtopic.replace('sensor/', '').strip()
                                            state['sensors'][sname] = vals
                    except zmq.Again:
                        pass

                if cam_socket in socks:
                    try:
                        for _ in range(20):
                            topic_bytes = cam_socket.recv(zmq.NOBLOCK)
                            topic = topic_bytes.decode('utf-8').strip()
                            if not cam_socket.getsockopt(zmq.RCVMORE):
                                continue
                            payload = cam_socket.recv()
                            parts = topic.split('/', 1)
                            if len(parts) >= 2:
                                prefix = parts[0]
                                subtopic = parts[1]
                                state = get_robot_state_locked(prefix)
                                state['hz']['msg_count'] += 1
                                if 'camera/' in subtopic:
                                    cam_name = subtopic.replace('camera/', '').strip()
                                    expected_size = args.res_x * args.res_y * 4
                                    if len(payload) == expected_size:
                                        img = np.frombuffer(payload, dtype=np.uint8)
                                        state['images'][cam_name] = np.copy(img).reshape((args.res_y, args.res_x, 4))
                    except zmq.Again:
                        pass
        except Exception as e:
            logger.error(f"ZMQ Thread Error: {e}")
            break


# ─── Policy Worker Thread ───

selected_policy_key = "unitree_12dof"


def policy_worker_thread(args):
    """Runs the selected RoboJuDo policy with twist forwarding from the GUI's ZMQ data."""
    import sys, traceback

    with policy_lock:
        pol_key = selected_policy_key
        policy_state["running"] = True
        policy_state["error"] = ""
        policy_state["step_count"] = 0
        policy_state["phase"] = "importing"

    # Capture the currently selected articulation prefix from the GUI
    target_prefix = selected_robot
    if target_prefix:
        logger.info(f"Policy will target articulation: '{target_prefix}'")
    else:
        logger.info("No articulation selected — policy will auto-detect")

    try:
        from urlab_policy.policy_registry import POLICIES, import_class
        from robojudo.pipeline.rl_pipeline import RlPipeline
        from robojudo.pipeline.pipeline_cfgs import RlPipelineCfg
        from urlab_policy.unreal_twist_ctrl import UnrealTwistCtrlCfg
        import urlab_policy.unreal_env

        pol_info = POLICIES[pol_key]

        with policy_lock:
            policy_state["name"] = pol_info["label"]
            policy_state["phase"] = "building pipeline"

        PolicyCfgClass = import_class(pol_info["policy_cfg"])
        EnvCfgClass = import_class(pol_info["env_cfg"])

        # Check motor mode toggle
        use_motor_pd = dpg.get_value("motor_mode_checkbox") if dpg.does_item_exist("motor_mode_checkbox") else False

        env_cfg = EnvCfgClass()
        env_cfg.state_endpoint = args.main_endpoint
        env_cfg.control_endpoint = args.control_endpoint

        # Use the GUI-selected articulation prefix instead of auto-detect
        if target_prefix:
            env_cfg.articulation_prefix = target_prefix

        policy_cfg = PolicyCfgClass()
        policy_cfg.freq = 50

        # Select controller based on policy type
        ctrl_list = []
        ctrl_type = pol_info.get("ctrl_type", "")
        if ctrl_type == "twist":
            ctrl_list = [UnrealTwistCtrlCfg()]
        elif ctrl_type == "motion_twist":
            from robojudo.config.g1.ctrl.g1_motion_ctrl_cfg import G1MotionTwistCtrlCfg
            ctrl_list = [UnrealTwistCtrlCfg(), G1MotionTwistCtrlCfg()]
        elif ctrl_type == "motion":
            from robojudo.config.g1.ctrl.g1_beyondmimic_ctrl_cfg import G1BeyondmimicCtrlCfg

            # Map motion dropdown to policy_name and observation config
            # Dance/Jump: 154 obs (without_state_estimator=True)
            # Violin/Waltz: 160 obs (without_state_estimator=False, needs odometry)
            MOTION_MAP = {
                "Dance":  {"policy_name": "Dance_wose",  "without_se": True},
                "Jump":   {"policy_name": "Jump_wose",   "without_se": True},
                "Violin": {"policy_name": "Violin",      "without_se": False},
                "Waltz":  {"policy_name": "Waltz",        "without_se": False},
            }
            selected_motion = dpg.get_value("motion_combo") if dpg.does_item_exist("motion_combo") else "Dance"
            motion_info = MOTION_MAP.get(selected_motion, MOTION_MAP["Dance"])
            policy_cfg.policy_name = motion_info["policy_name"]
            policy_cfg.without_state_estimator = motion_info["without_se"]
            logger.info(f"BeyondMimic motion: {selected_motion} -> policy={motion_info['policy_name']}, without_SE={motion_info['without_se']}")

            ctrl_list = [G1BeyondmimicCtrlCfg()]
        elif ctrl_type == "motion_h2h":
            from robojudo.config.g1.ctrl.g1_motion_ctrl_cfg import G1MotionH2HCtrlCfg
            ctrl_list = [G1MotionH2HCtrlCfg()]

        pipeline_cfg = RlPipelineCfg(
            robot="g1",
            env=env_cfg,
            ctrl=ctrl_list,
            policy=policy_cfg,
        )

        pipeline = RlPipeline(cfg=pipeline_cfg)

        # If motor+PD mode, push gains to Unreal's UMjPDController
        if use_motor_pd:
            logger.info("Motor+PD mode: pushing gains to Unreal")
            kp = np.asarray(pipeline.env.stiffness, dtype=np.float32)
            kv = np.asarray(pipeline.env.damping, dtype=np.float32)
            tl = np.asarray(pipeline.env.torque_limits, dtype=np.float32) if pipeline.env.torque_limits is not None else np.full(pipeline.env.num_dofs, 200.0, dtype=np.float32)
            for _ in range(10):
                pipeline.env.zmq.send_gains(pipeline.env.prefix, pipeline.env.joint_names, kp, kv, tl)
                time.sleep(0.05)
            logger.info(f"Pushed motor PD gains: Kp[0]={kp[0]:.2f}")

        with policy_lock:
            policy_state["joint_mapping"] = dict(pipeline.env._zmq_id_to_dof)
            policy_state["phase"] = "preparing"

        # Wait for valid base state before starting (prevents bad alignment on first frame)
        logger.info("Waiting for valid base state from ZMQ...")
        base_wait_start = time.time()
        while time.time() - base_wait_start < 5.0:
            pipeline.env.update(simple=True)
            if pipeline.env._base_state_received:
                pos = pipeline.env._base_pos
                if not np.allclose(pos, 0.0):
                    logger.info(f"Base state valid: pos={pos}")
                    break
            time.sleep(0.05)
        else:
            logger.warning("Timed out waiting for valid base state — proceeding anyway")

        # Phase 1: Stabilize at frame 0 for 5 seconds
        # The policy actively balances while holding the init pose.
        # This lets the robot fully settle before we lock in the alignment.
        logger.info("Stabilizing with policy at frame 0 (250 steps / 5s)...")
        pipeline.reset()

        if hasattr(pipeline.policy, 'play_speed'):
            pipeline.policy.play_speed = 0.0

        for i in range(250):
            pipeline.step()
            time.sleep(0.02)

        logger.info("Stabilization done — recomputing alignment from settled pose")

        # Phase 2: Recompute alignment from the now-stable base state
        # The robot is standing still at frame 0 — this is the ideal moment
        # to capture the reference frame for the motion.
        pipeline.env.update(simple=True)
        pipeline.reset()

        # Re-freeze at frame 0 briefly to fill history with aligned observations
        if hasattr(pipeline.policy, 'play_speed'):
            pipeline.policy.play_speed = 0.0

        for i in range(50):
            pipeline.step()
            time.sleep(0.02)

        # Phase 3: Resume playback
        if hasattr(pipeline.policy, 'play_speed'):
            pipeline.policy.play_speed = 1.0
            logger.info("Alignment locked — resuming playback")

        with policy_lock:
            policy_state["phase"] = "running"

        dt = 1.0 / 50
        while True:
            with policy_lock:
                if not policy_state["running"]:
                    break

            # Check for pending gait preset change
            with policy_lock:
                pending_gait = policy_state.get("pending_gait_preset")
                if pending_gait:
                    policy_state["pending_gait_preset"] = None
            if pending_gait and hasattr(pipeline.policy, 'set_gait_preset'):
                pipeline.policy.set_gait_preset(pending_gait)

            # BeyondMimic motion looping — detect when motion output freezes and restart
            if hasattr(pipeline.policy, 'command') and pipeline.policy.command is not None:
                should_loop = (dpg.get_value("loop_motion_checkbox")
                               if dpg.does_item_exist("loop_motion_checkbox") else False)
                if should_loop:
                    current_jp = pipeline.policy.command.get("joint_pos")
                    if current_jp is not None:
                        if not hasattr(pipeline.policy, '_prev_motion_jp'):
                            pipeline.policy._prev_motion_jp = current_jp.copy()
                            pipeline.policy._motion_stale_count = 0
                        else:
                            diff = np.max(np.abs(current_jp - pipeline.policy._prev_motion_jp))
                            if diff < 1e-4:
                                pipeline.policy._motion_stale_count += 1
                            else:
                                pipeline.policy._motion_stale_count = 0
                            pipeline.policy._prev_motion_jp = current_jp.copy()

                            if pipeline.policy._motion_stale_count > 50:  # ~1 second of no change
                                logger.info(f"Motion frozen for {pipeline.policy._motion_stale_count} steps — looping")
                                pipeline.policy._motion_stale_count = 0
                                pipeline.policy.reset()
                                pipeline.reset()

            # Force twist override from GUI sliders
            if (dpg.does_item_exist("force_twist_checkbox") and
                    dpg.get_value("force_twist_checkbox")):
                vx = dpg.get_value("twist_vx") if dpg.does_item_exist("twist_vx") else 0.0
                vy = dpg.get_value("twist_vy") if dpg.does_item_exist("twist_vy") else 0.0
                yaw = dpg.get_value("twist_yaw") if dpg.does_item_exist("twist_yaw") else 0.0
                pipeline.env._force_twist = np.array([vx, vy, yaw], dtype=np.float32)
            else:
                pipeline.env._force_twist = None

            step_start = time.time()
            pipeline.step()

            with policy_lock:
                step_count = policy_state["step_count"] + 1
                policy_state["step_count"] = step_count
                if hasattr(pipeline.policy, "last_action"):
                    policy_state["last_action"] = np.copy(pipeline.policy.last_action)
                policy_state["last_pd_target"] = np.copy(pipeline.env.dof_pos)

            # Diagnostic logging every 250 steps (~5 seconds)
            if step_count % 250 == 0:
                env = pipeline.env
                pol = pipeline.policy
                action_rms = np.sqrt(np.mean(pol.last_action**2)) if hasattr(pol, 'last_action') else 0
                dof_err = 0
                if hasattr(pol, 'default_dof_pos') and len(env.dof_pos) == len(pol.default_dof_pos):
                    dof_err = np.max(np.abs(env.dof_pos - pol.default_dof_pos))
                base_pos = env._base_pos
                base_quat = env._base_quat
                lin_vel = env._base_lin_vel
                ang_vel = env._base_ang_vel
                torso_pos = env._torso_pos if hasattr(env, '_torso_pos') else np.zeros(3)
                ts = pol.timestep if hasattr(pol, 'timestep') else -1
                logger.info(
                    f"[DIAG step={step_count}] ts={ts:.0f} "
                    f"action_rms={action_rms:.3f} dof_err={dof_err:.3f} "
                    f"base_pos=[{base_pos[0]:.3f},{base_pos[1]:.3f},{base_pos[2]:.3f}] "
                    f"base_quat=[{base_quat[0]:.3f},{base_quat[1]:.3f},{base_quat[2]:.3f},{base_quat[3]:.3f}] "
                    f"lin_vel=[{lin_vel[0]:.3f},{lin_vel[1]:.3f},{lin_vel[2]:.3f}] "
                    f"ang_vel=[{ang_vel[0]:.3f},{ang_vel[1]:.3f},{ang_vel[2]:.3f}] "
                    f"torso=[{torso_pos[0]:.3f},{torso_pos[1]:.3f},{torso_pos[2]:.3f}]"
                )

            sleep_time = dt - (time.time() - step_start)
            if sleep_time > 0:
                time.sleep(sleep_time)

            with policy_lock:
                total_elapsed = time.time() - step_start
                policy_state["freq_hz"] = 1.0 / max(total_elapsed, 0.001)

    except Exception as e:
        tb = traceback.format_exc()
        logger.error(f"Policy error:\n{tb}")
        with policy_lock:
            policy_state["error"] = str(e)
            policy_state["phase"] = "error"
    finally:
        with policy_lock:
            policy_state["running"] = False
            if not policy_state["error"]:
                policy_state["phase"] = "stopped"


# ─── UI ───

def on_slider_changed(sender, app_data, user_data):
    control_queue.put((user_data[0], user_data[1], app_data))

built_ui_for_robot = None
policy_thread = None


def create_ui(args):
    dpg.create_context()
    dpg.create_viewport(title='URLab Debug Dashboard', width=1100, height=850)
    dpg.setup_dearpygui()

    with dpg.texture_registry(show=False):
        blank = np.zeros((480, 640, 4), dtype=np.float32).flatten()
        dpg.add_dynamic_texture(width=640, height=480, default_value=blank, tag="camera_texture")

    with dpg.window(label="URLab Dashboard", tag="main_window"):
        with dpg.group(horizontal=True):
            # Left sidebar
            with dpg.child_window(width=220, height=-1):
                dpg.add_text("Articulations:")

                def on_robot_select(sender, app_data):
                    global selected_robot, selected_camera
                    selected_robot = app_data
                    selected_camera = None

                dpg.add_listbox([], tag="robot_list", width=-1, num_items=8, callback=on_robot_select)

                dpg.add_separator()
                dpg.add_text("ZMQ Status:")
                dpg.add_text("Waiting...", tag="status_text")
                dpg.add_text("Rate: 0 Hz", tag="rate_text")

                dpg.add_separator()
                with dpg.collapsing_header(label="Policy Control", default_open=True):
                    from urlab_policy.policy_registry import POLICIES, get_policy_labels
                    labels = get_policy_labels()
                    label_to_key = {v: k for k, v in labels.items()}
                    policy_items = list(labels.values())

                    def _update_policy_options(pol_key):
                        """Show/hide motion, gait, and twist selectors based on policy type."""
                        info = POLICIES.get(pol_key, {})
                        ctrl_type = info.get("ctrl_type", "")
                        is_wtw = "wtw" in pol_key.lower()
                        is_beyondmimic = ctrl_type == "motion"
                        is_twist = ctrl_type in ("twist", "motion_twist") or is_wtw

                        if dpg.does_item_exist("motion_group"):
                            dpg.configure_item("motion_group", show=is_beyondmimic)
                        if dpg.does_item_exist("gait_group"):
                            dpg.configure_item("gait_group", show=is_wtw)
                        if dpg.does_item_exist("twist_group"):
                            dpg.configure_item("twist_group", show=is_twist)

                    def on_policy_select(sender, app_data):
                        global selected_policy_key
                        selected_policy_key = label_to_key.get(app_data, "unitree_12dof")
                        info = POLICIES[selected_policy_key]
                        if dpg.does_item_exist("policy_desc_text"):
                            dpg.set_value("policy_desc_text", f"{info['desc']}\nDOFs: {info['dofs']}  |  XML: {info['xml']}")
                        _update_policy_options(selected_policy_key)

                    dpg.add_combo(policy_items, default_value=policy_items[0],
                                  callback=on_policy_select, width=-1, tag="policy_combo")
                    dpg.add_text(f"{POLICIES['unitree_12dof']['desc']}\nDOFs: 12  |  XML: g1_12dof",
                                 tag="policy_desc_text", wrap=200, color=(180, 180, 180))

                    # BeyondMimic motion selector (only visible when BeyondMimic policy selected)
                    with dpg.group(tag="motion_group", show=False):
                        dpg.add_spacer(height=3)
                        motion_items = ["Dance", "Jump", "Violin", "Waltz"]
                        dpg.add_combo(motion_items, default_value="Dance", width=-1,
                                      tag="motion_combo", label="Motion")
                        dpg.add_checkbox(label="Loop Motion", tag="loop_motion_checkbox",
                                         default_value=True)

                    # WTW gait preset selector (only visible when WTW policy selected)
                    with dpg.group(tag="gait_group", show=False):
                        dpg.add_spacer(height=3)
                        gait_items = ["Trot", "Relaxed Trot", "High Step Trot", "Relaxed High",
                                      "Fast Trot", "Pronk", "Bound", "Pace", "Low Crouch", "Wide Stance"]

                        def on_gait_select(sender, app_data):
                            with policy_lock:
                                policy_state["pending_gait_preset"] = app_data

                        dpg.add_combo(gait_items, default_value="Trot", width=-1,
                                      callback=on_gait_select, tag="gait_combo", label="Gait")

                    # Force twist override (visible for twist-based policies)
                    with dpg.group(tag="twist_group", show=False):
                        dpg.add_spacer(height=3)
                        dpg.add_separator()
                        dpg.add_checkbox(label="Force Twist", tag="force_twist_checkbox",
                                         default_value=False)
                        dpg.add_text("Vx (m/s):", color=(180, 180, 180))
                        dpg.add_slider_float(tag="twist_vx", default_value=0.0,
                                             min_value=-1.5, max_value=1.5, width=-1)
                        dpg.add_text("Vy (m/s):", color=(180, 180, 180))
                        dpg.add_slider_float(tag="twist_vy", default_value=0.0,
                                             min_value=-1.0, max_value=1.0, width=-1)
                        dpg.add_text("Yaw (rad/s):", color=(180, 180, 180))
                        dpg.add_slider_float(tag="twist_yaw", default_value=0.0,
                                             min_value=-2.0, max_value=2.0, width=-1)

                    dpg.add_spacer(height=5)
                    dpg.add_text("Stopped", tag="policy_status_text", color=(255, 100, 100))
                    dpg.add_text("Phase: idle", tag="policy_phase_text")
                    dpg.add_text("", tag="policy_freq_text")
                    dpg.add_text("", tag="policy_steps_text")
                    dpg.add_text("", tag="policy_error_text", color=(255, 80, 80), wrap=200)

                    dpg.add_spacer(height=5)

                    def on_start_policy(sender):
                        global policy_thread
                        if policy_thread and policy_thread.is_alive():
                            return
                        policy_thread = threading.Thread(target=policy_worker_thread, args=(args,), daemon=True)
                        policy_thread.start()

                    def on_stop_policy(sender):
                        with policy_lock:
                            policy_state["running"] = False

                    dpg.add_button(label="Start Policy", callback=on_start_policy, width=-1, tag="btn_start")
                    dpg.add_button(label="Stop Policy", callback=on_stop_policy, width=-1, tag="btn_stop")

            # Main content
            with dpg.child_window(width=-1, height=-1):
                with dpg.tab_bar():
                    with dpg.tab(label="Joints"):
                        dpg.add_input_text(tag="joints_text", multiline=True, width=-1, height=-1, readonly=True)

                    with dpg.tab(label="Sensors"):
                        dpg.add_input_text(tag="sensors_text", multiline=True, width=-1, height=-1, readonly=True)

                    with dpg.tab(label="Control"):
                        with dpg.child_window(tag="control_panel", width=-1, height=-1):
                            dpg.add_text("Waiting for actuator config...", tag="waiting_text")

                    with dpg.tab(label="Twist / Base"):
                        dpg.add_input_text(tag="twist_text", multiline=True, width=-1, height=-1, readonly=True)

                    with dpg.tab(label="Policy Output"):
                        dpg.add_input_text(tag="policy_output_text", multiline=True, width=-1, height=-1, readonly=True)

                    with dpg.tab(label="Cameras"):
                        def on_cam_select(sender, app_data):
                            global selected_camera
                            selected_camera = app_data
                        dpg.add_combo([], label="Camera", tag="camera_select", callback=on_cam_select)
                        dpg.add_spacer(height=5)
                        dpg.add_image("camera_texture")

    dpg.set_primary_window("main_window", True)


def ui_update_loop():
    global selected_robot, selected_camera, built_ui_for_robot

    with state_lock:
        robots = list(robot_states.keys())
        if dpg.does_item_exist("robot_list"):
            dpg.configure_item("robot_list", items=robots)
            if not selected_robot and robots:
                selected_robot = robots[0]
                dpg.set_value("robot_list", selected_robot)

        if not selected_robot or selected_robot not in robot_states:
            return

        state = robot_states[selected_robot]

        # Hz
        t = time.time()
        dt = t - state['hz']['start_time']
        if dt >= 1.0:
            state['hz']['overall'] = state['hz']['msg_count'] / dt
            state['hz']['msg_count'] = 0
            state['hz']['start_time'] = t
        dpg.set_value("rate_text", f"Rate: {state['hz']['overall']:.0f} Hz")

        # Control sliders
        config = state['actuator_config']
        if config and built_ui_for_robot != selected_robot:
            if dpg.does_item_exist("control_panel"):
                dpg.delete_item("control_panel", children_only=True)
                names = config.get('names', [])
                ids = config.get('ids', [])
                mins = config.get('mins', [])
                maxs = config.get('maxs', [])
                for idx, act_name in enumerate(names):
                    display = act_name.replace(f"{selected_robot}_", "")
                    dpg.add_slider_float(
                        label=display, default_value=0.0,
                        min_value=mins[idx], max_value=maxs[idx],
                        callback=on_slider_changed,
                        user_data=(selected_robot, ids[idx]),
                        parent="control_panel",
                    )
            built_ui_for_robot = selected_robot

        # Joints tab
        jt = "ID  Name                    Pos        Vel        Acc\n" + "=" * 65 + "\n"
        for jname in sorted(state['joints'].keys()):
            jid, p, v, a = state['joints'][jname]
            jt += f"{jid:2.0f}  {jname[:22]:<22s}  {p:+8.4f}   {v:+8.4f}   {a:+8.4f}\n"
        if dpg.does_item_exist("joints_text"):
            dpg.set_value("joints_text", jt)

        # Sensors tab
        st = "Name                    Dim  Data\n" + "=" * 60 + "\n"
        for sname in sorted(state['sensors'].keys()):
            vals = state['sensors'][sname]
            data_str = "  ".join(f"{v:.3f}" for v in vals[:6])
            if len(vals) > 6:
                data_str += " ..."
            st += f"{sname[:22]:<22s}  {len(vals):3d}  {data_str}\n"
        if dpg.does_item_exist("sensors_text"):
            dpg.set_value("sensors_text", st)

        # Twist / Base tab
        tt = "═══ TWIST COMMAND ═══\n"
        vx, vy, yr = state['twist']
        tt += f"  Vx (forward):   {vx:+.4f} m/s\n"
        tt += f"  Vy (strafe):    {vy:+.4f} m/s\n"
        tt += f"  Yaw rate:       {yr:+.4f} rad/s\n"
        active = "ACTIVE" if abs(vx) > 0.01 or abs(vy) > 0.01 or abs(yr) > 0.01 else "idle"
        tt += f"  Status:         {active}\n\n"

        actions = state['actions']
        if actions:
            pressed = [str(i) for i in range(10) if actions & (1 << i)]
            tt += f"═══ ACTION KEYS ═══\n  Pressed: [{', '.join(pressed)}]\n\n"

        bs = state.get('base_state')
        if bs:
            tt += "═══ BASE STATE ═══\n"
            tt += f"  Position:    {bs['pos'][0]:+.3f}  {bs['pos'][1]:+.3f}  {bs['pos'][2]:+.3f}\n"
            tt += f"  Quaternion:  {bs['quat'][0]:+.4f}  {bs['quat'][1]:+.4f}  {bs['quat'][2]:+.4f}  {bs['quat'][3]:+.4f}\n"
            tt += f"  Lin Vel:     {bs['linvel'][0]:+.3f}  {bs['linvel'][1]:+.3f}  {bs['linvel'][2]:+.3f}\n"
            tt += f"  Ang Vel:     {bs['angvel'][0]:+.3f}  {bs['angvel'][1]:+.3f}  {bs['angvel'][2]:+.3f}\n"
        else:
            tt += "═══ BASE STATE ═══\n  No free joint data\n"

        if dpg.does_item_exist("twist_text"):
            dpg.set_value("twist_text", tt)

        # Camera
        cameras = list(state['images'].keys())
        if dpg.does_item_exist("camera_select"):
            cur = dpg.get_item_configuration("camera_select")['items']
            if sorted(cur) != sorted(cameras):
                dpg.configure_item("camera_select", items=cameras)
                if not selected_camera and cameras:
                    selected_camera = cameras[0]
                    dpg.set_value("camera_select", selected_camera)
        if selected_camera and selected_camera in state['images']:
            img = state['images'][selected_camera]
            try:
                dpg_img = img.astype(np.float32) / 255.0
                temp = np.copy(dpg_img[:, :, 0])
                dpg_img[:, :, 0] = dpg_img[:, :, 2]
                dpg_img[:, :, 2] = temp
                dpg.set_value("camera_texture", dpg_img.flatten())
                dpg.set_value("status_text", f"Streaming: {selected_camera}")
            except Exception as e:
                dpg.set_value("status_text", f"Cam error: {e}")
        else:
            dpg.set_value("status_text", f"Connected ({len(state['joints'])} joints)")

    # Policy panel
    with policy_lock:
        if policy_state["running"]:
            dpg.set_value("policy_status_text", f"Running: {policy_state['name']}")
            dpg.configure_item("policy_status_text", color=(100, 255, 100))
        else:
            dpg.set_value("policy_status_text", "Stopped")
            dpg.configure_item("policy_status_text", color=(255, 100, 100))

        dpg.set_value("policy_phase_text", f"Phase: {policy_state['phase']}")
        dpg.set_value("policy_freq_text", f"Freq: {policy_state['freq_hz']:.1f} Hz")
        dpg.set_value("policy_steps_text", f"Steps: {policy_state['step_count']}")

        if policy_state["error"]:
            dpg.set_value("policy_error_text", policy_state["error"][:120])
        else:
            dpg.set_value("policy_error_text", "")

        # Policy output tab
        po = f"Policy: {policy_state['name']}\n"
        po += f"Phase: {policy_state['phase']}  |  Steps: {policy_state['step_count']}  |  Freq: {policy_state['freq_hz']:.1f} Hz\n"
        po += "=" * 55 + "\n"

        if policy_state["last_action"] is not None:
            po += "\nLast Action (raw policy output):\n"
            for i, v in enumerate(policy_state["last_action"]):
                po += f"  [{i:2d}] {v:+.5f}\n"

        if policy_state["last_pd_target"] is not None:
            po += "\nCurrent DOF Pos (from env):\n"
            for i, v in enumerate(policy_state["last_pd_target"]):
                po += f"  [{i:2d}] {v:+.5f}\n"

        if policy_state["joint_mapping"]:
            po += f"\nJoint Mapping (ZMQ_ID -> DOF):\n"
            for zmq_id, dof_idx in sorted(policy_state["joint_mapping"].items()):
                po += f"  ZMQ {zmq_id:2d} -> DOF {dof_idx:2d}\n"

        if policy_state["error"]:
            po += f"\nERROR:\n{policy_state['error']}\n"

        if dpg.does_item_exist("policy_output_text"):
            dpg.set_value("policy_output_text", po)


def main():
    parser = argparse.ArgumentParser(description="URLab Debug Dashboard")
    parser.add_argument('--main_endpoint', default="tcp://127.0.0.1:5555")
    parser.add_argument('--camera_endpoint', default="tcp://127.0.0.1:5558")
    parser.add_argument('--control_endpoint', default="tcp://127.0.0.1:5556")
    parser.add_argument('--info_endpoint', default="tcp://127.0.0.1:5557")
    parser.add_argument('--res_x', type=int, default=640)
    parser.add_argument('--res_y', type=int, default=480)
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s — %(message)s", datefmt="%H:%M:%S")

    zmq_thread = threading.Thread(target=zmq_worker_thread, args=(args,), daemon=True)
    zmq_thread.start()

    create_ui(args)
    dpg.show_viewport()

    last_update = time.time()
    while dpg.is_dearpygui_running():
        if (time.time() - last_update) >= (1.0 / 30.0):
            ui_update_loop()
            last_update = time.time()
        dpg.render_dearpygui_frame()
    dpg.destroy_context()


if __name__ == "__main__":
    main()
