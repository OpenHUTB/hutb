import logging
import time

import numpy as np
from zed_proxy import ZedOdometryProxy

from robojudo.tools.tool_cfgs import ZedOdometryCfg
from robojudo.utils.rotation import TransformAlignment
from robojudo.utils.util_func import quat_rotate_inverse_np

logger = logging.getLogger(__name__)


class ZedOdometry:
    def __init__(self, cfg: ZedOdometryCfg):
        self.cfg = cfg
        self.zed_proxy = ZedOdometryProxy(server_ip=cfg.server_ip)
        self.zero_align = TransformAlignment(yaw_only=True, xy_only=True)

        result_msg = self.zed_proxy.enable_sensor()  # sync block
        logger.info(f"[ZedOdometry] enable_sensor -> {result_msg}")

        self.staus_data = {}

        self._valid = False
        self._pos = np.zeros(3)
        self._quat = np.array([0.0, 0.0, 0.0, 1.0])  # xyzw
        self._rpy = np.zeros(3)
        self._lin_vel = np.zeros(3)

        if not self.self_check():
            raise RuntimeError("[ZedOdometry] self_check failed")

        if self.cfg.zero_align:
            self.set_zreo()

    def self_check(self):
        for _ in range(20):
            self.update()
            if self._valid:
                return True
            else:
                logger.warning("[ZedOdometry] self_check: data not valid, retry...")
            time.sleep(0.1)
        logger.error("[ZedOdometry] self_check failed: data not valid")
        return False

    def set_zreo(self):
        if not self._valid:
            logger.error("[ZedOdometry] set_zero failed: data not valid")
            return
        pos = self.staus_data["position_xyz"]
        quat = self.staus_data["quat"]
        self.zero_align.set_base(quat, pos)

    def update(self):
        self.staus_data = self.zed_proxy.get_status()
        if self.staus_data is None or len(self.staus_data) == 0:
            self._valid = False
            logger.warning("[Warning][ZedOdometry] update: no data")
            return
        if not self.staus_data.get("flag_ok", False):
            self._valid = False
            logger.warning("[Warning][ZedOdometry] update: data not valid")
            return

        self._valid = True
        if pos := self.staus_data.get("position_xyz", None):
            _pos = np.array(pos)
            if self.cfg.zero_align:
                _pos = self.zero_align.align_pos(_pos)
            self._pos = _pos + np.array(self.cfg.pos_offset)

        if quat := self.staus_data.get("quat", None):
            _quat = np.array(quat)
            if self.cfg.zero_align:
                _quat = self.zero_align.align_quat(_quat)
            self._quat = _quat

        if rpy := self.staus_data.get("rpy", None):
            self._rpy = np.array(rpy)

        if lin_vel := self.staus_data.get("vel_xyz_world", None):
            _lin_vel = np.array(lin_vel)
            self._lin_vel = quat_rotate_inverse_np(self._quat, _lin_vel)

    @property
    def is_valid(self):
        return self._valid

    @property
    def pos(self):
        return self._pos.copy()

    @property
    def quat(self):
        return self._quat.copy()  # xyzw

    @property
    def rpy(self):
        return self._rpy.copy()

    @property
    def lin_vel(self):
        return self._lin_vel.copy()

    def __del__(self):
        self.zed_proxy.close()


if __name__ == "__main__":
    cfg = ZedOdometryCfg(server_ip="192.168.24.102")
    zed_odom = ZedOdometry(cfg)

    import time

    while True:
        zed_odom.update()
        if zed_odom.is_valid:
            print(f"pos: {zed_odom.pos}, rpy: {zed_odom.rpy}, lin_vel: {zed_odom.lin_vel}")
        else:
            print("data not valid")
        time.sleep(0.1)
