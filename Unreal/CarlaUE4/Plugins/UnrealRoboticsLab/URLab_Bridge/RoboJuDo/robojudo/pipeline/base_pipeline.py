import logging
from abc import ABC, abstractmethod

from robojudo.tools.debug_log import DebugLogger

from .pipeline_cfgs import PipelineCfg

logger = logging.getLogger(__name__)


class Pipeline(ABC):
    """
    Base Controller Module
    """

    def __init__(self, cfg: PipelineCfg):
        self.cfg = cfg

        if cfg.device == "auto":
            import torch

            self.device = "cuda" if torch.cuda.is_available() else "cpu"
        else:
            self.device = cfg.device
        logger.info(f"Using device: {self.device}")

        if self.cfg.debug.log_obs:
            self.debug_logger = DebugLogger(run_cfg=cfg)

        self.dt = 1.0 / 50  # default
        self.timestep = 0
        self.do_safety_check = self.cfg.do_safety_check

    @abstractmethod
    def step(self):
        raise NotImplementedError

    @abstractmethod
    def prepare(self):
        raise NotImplementedError

    def safety_check(self):
        return
