# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
# Licensed under the Apache License, Version 2.0.

"""
URLab Bridge runner -- dashboard, headless policy, or connection test.

Usage:
    # Launch the dashboard (joints, sensors, cameras, optional policy control):
    uv run src/run.py --ui

    # Run a policy headless:
    uv run src/run.py --policy unitree --prefix g1

    # Test ZMQ connection:
    uv run src/run.py --test --prefix g1
"""

import argparse
import logging
import sys
import time

logger = logging.getLogger(__name__)


def main():
    parser = argparse.ArgumentParser(description="URLab Policy Runner")
    parser.add_argument("--ui", action="store_true", help="Launch debug UI")
    parser.add_argument("--test", action="store_true", help="Test ZMQ connection")
    parser.add_argument("--policy", type=str, default=None,
                        help="Policy to run (unitree_12dof, smooth, beyondmimic_dance, h2h, amo, twist_tracker, etc.)")
    parser.add_argument("--prefix", type=str, default="g1",
                        help="Articulation prefix in Unreal")
    parser.add_argument("--state-ep", type=str, default="tcp://127.0.0.1:5555")
    parser.add_argument("--ctrl-ep", type=str, default="tcp://127.0.0.1:5556")
    parser.add_argument("--freq", type=int, default=50, help="Policy frequency (Hz)")
    parser.add_argument("--twist-source", type=str, default="keyboard",
                        choices=["keyboard", "zmq"],
                        help="Twist command source: keyboard (Python pynput) or zmq (Unreal input)")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(name)s %(levelname)s — %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.ui:
        import sys
        sys.argv = [a for a in sys.argv if a != "--ui"]
        from urlab_policy.dashboard import main as run_dashboard
        run_dashboard()
        return

    if args.test:
        _test_connection(args)
        return

    if args.policy:
        _run_policy(args)
        return

    parser.print_help()


def _test_connection(args):
    """Print received joint data for 10 seconds."""
    import struct
    import zmq

    logger.info(f"Testing {args.state_ep} (prefix: {args.prefix})")

    ctx = zmq.Context()
    sub = ctx.socket(zmq.SUB)
    sub.connect(args.state_ep)
    sub.setsockopt_string(zmq.SUBSCRIBE, "")
    sub.setsockopt(zmq.RCVTIMEO, 5000)

    count = 0
    start = time.time()
    try:
        while time.time() - start < 10:
            try:
                topic = sub.recv_string()
                if sub.getsockopt(zmq.RCVMORE):
                    payload = sub.recv()
                    if "/joint/" in topic and len(payload) == 16:
                        jid, p, v, a = struct.unpack("<Ifff", payload)
                        print(f"  [{topic}] ID:{jid} Pos:{p:.3f} Vel:{v:.3f}")
                        count += 1
            except zmq.Again:
                if count == 0:
                    logger.warning("No data yet...")
    except KeyboardInterrupt:
        pass
    finally:
        sub.close()
        ctx.term()
    logger.info(f"Received {count} messages in {time.time()-start:.1f}s")


def _run_policy(args):
    """Run a RoboJuDo policy pipeline with UnrealEnv."""
    try:
        from robojudo.pipeline.rl_pipeline import RlPipeline, PolicyWrapper
        from robojudo.controller.ctrl_cfgs import KeyboardCtrlCfg
        from robojudo.pipeline.pipeline_cfgs import RlPipelineCfg
        from robojudo.config.g1.policy.g1_unitree_policy_cfg import (
            G1UnitreePolicyCfg,
            G1UnitreeWoGaitPolicyCfg,
        )
        from robojudo.config.g1.policy.g1_amo_policy_cfg import (
            G1AmoPolicyCfg,
        )
    except ImportError:
        logger.error(
            "RoboJuDo not installed. Run:\n"
            "  cd urlab_bridge/RoboJuDo && pip install -e ."
        )
        sys.exit(1)

    from urlab_policy.env_config import G1UnrealEnvCfg
    import urlab_policy.unreal_env  # registers UnrealEnv with env_registry

    # Build config
    env_cfg = G1UnrealEnvCfg()
    env_cfg.state_endpoint = args.state_ep
    env_cfg.control_endpoint = args.ctrl_ep
    env_cfg.articulation_prefix = args.prefix

    if args.policy == "unitree":
        policy_cfg = G1UnitreePolicyCfg()
    elif args.policy == "unitree_nogait":
        policy_cfg = G1UnitreeWoGaitPolicyCfg()
    elif args.policy == "amo":
        policy_cfg = G1AmoPolicyCfg()
    else:
        logger.error(f"Unknown policy: {args.policy}")
        sys.exit(1)

    policy_cfg.freq = args.freq

    # Choose controller based on twist source
    if args.twist_source == "zmq":
        from urlab_policy.unreal_twist_ctrl import UnrealTwistCtrlCfg
        ctrl_cfgs = [UnrealTwistCtrlCfg()]
    else:
        ctrl_cfgs = [KeyboardCtrlCfg()]

    pipeline_cfg = RlPipelineCfg(
        robot="g1",
        env=env_cfg,
        ctrl=ctrl_cfgs,
        policy=policy_cfg,
    )

    logger.info(f"Initializing pipeline: {args.policy} @ {args.freq}Hz → {args.prefix} (twist: {args.twist_source})")
    pipeline = RlPipeline(cfg=pipeline_cfg)

    logger.info("Preparing robot (moving to init pose)...")
    pipeline.prepare()

    logger.info("Running policy loop — Ctrl+C to stop")
    dt = 1.0 / args.freq
    try:
        while True:
            step_start = time.time()
            pipeline.step()

            elapsed = time.time() - step_start
            sleep_time = dt - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)
    except KeyboardInterrupt:
        logger.info("Stopping...")
    finally:
        pipeline.env.shutdown()
        logger.info("Done")


if __name__ == "__main__":
    main()
