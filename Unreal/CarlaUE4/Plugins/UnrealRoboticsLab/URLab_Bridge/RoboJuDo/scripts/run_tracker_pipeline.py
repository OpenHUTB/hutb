"""Run a ProtoMotions tracker policy.

Extends the generic pipeline runner with tracker-specific CLI overrides
for ONNX model, motion file, and motion index.

Usage::

    python scripts/run_tracker_pipeline.py -c g1_protomotions_tracker \\
        --onnx-path /path/to/unified_pipeline.onnx \\
        --motion-path /path/to/motion.motion
"""

# Fix OMP perfmance issue on ARM platform (Jetson)
import os
import platform

if platform.machine().startswith("aarch64"):
    os.environ["OMP_NUM_THREADS"] = "1"

import argparse
import logging
import time

import robojudo.pipeline
from robojudo.config.config_manager import ConfigManager
from robojudo.pipeline.pipeline_cfgs import RlPipelineCfg
from robojudo.pipeline.rl_pipeline import RlPipeline

logger = logging.getLogger("robojudo")


def parse_args():
    parser = argparse.ArgumentParser(description="Run a ProtoMotions tracker policy")
    parser.add_argument(
        "-c",
        "--config",
        type=str,
        default="g1_protomotions_tracker",
        help="Name of the config class to use",
    )
    parser.add_argument(
        "--onnx-path",
        type=str,
        default=None,
        help="Path to the ONNX policy file (overrides config)",
    )
    parser.add_argument(
        "--motion-path",
        type=str,
        default=None,
        help="Path to the motion file (overrides config)",
    )
    parser.add_argument(
        "--motion-index",
        type=int,
        default=None,
        help="Index of motion clip within a multi-motion .pt library",
    )
    args = parser.parse_args()
    return args


def main():
    args = parse_args()
    logger.info(f"Using config: {args.config}")
    config_manager = ConfigManager(config_name=args.config)

    cfg: RlPipelineCfg = config_manager.get_cfg()

    # Override policy paths from CLI arguments.
    if args.onnx_path is not None and hasattr(cfg, "policy"):
        cfg.policy.onnx_path = args.onnx_path
    if args.motion_path is not None and hasattr(cfg, "policy"):
        cfg.policy.motion_path = args.motion_path
    if args.motion_index is not None and hasattr(cfg, "policy"):
        cfg.policy.motion_index = args.motion_index

    pipeline_type = cfg.pipeline_type

    pipeline_class: type[RlPipeline] = getattr(robojudo.pipeline, pipeline_type)
    logger.info(f"Using pipeline: {pipeline_type} -> {pipeline_class}")

    pipeline = pipeline_class(cfg=cfg)

    if not cfg.env.is_sim:
        pipeline.prepare()

    while True:
        time_start = time.time()
        pipeline.step()
        time_end = time.time()
        time_diff = time_end - time_start

        # keep the pipeline running at the desired frequency
        if not cfg.run_fullspeed:
            time_diff = pipeline.dt - time_diff
            if time_diff > 0:
                time.sleep(time_diff)
            else:
                if not cfg.env.is_sim:
                    logger.error(f"Warning: frame drop -> {time_diff}")
                    if time_diff < -0.2:
                        logger.critical("Exiting due to excessive frame drop")
                        pipeline.env.shutdown()
                        time.sleep(10)
                        break


if __name__ == "__main__":
    main()
