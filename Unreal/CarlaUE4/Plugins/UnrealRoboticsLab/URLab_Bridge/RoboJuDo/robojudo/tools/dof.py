import logging

import numpy as np

from .tool_cfgs import DoFConfig

logger = logging.getLogger(__name__)


def merge_dof_cfgs(base_cfg: DoFConfig, override_cfg: DoFConfig) -> DoFConfig:
    """
    Merge two DoFConfig objects, with override_cfg taking precedence over base_cfg.
    Only non-None values in override_cfg will replace those in base_cfg.

    Returns a new DoFConfig object.
    """
    if not isinstance(base_cfg, DoFConfig) or not isinstance(override_cfg, DoFConfig):
        raise ValueError("Both base_cfg and override_cfg must be instances of DoFConfig")

    merged_cfg = base_cfg.model_copy()

    dof_adapter = DoFAdapter(src_joint_names=override_cfg.joint_names, tar_joint_names=merged_cfg.joint_names)
    for key in override_cfg.prop_keys:
        value_override = getattr(override_cfg, key)
        if key in ["joint_names"] or value_override is None:
            continue
        if key not in merged_cfg.prop_keys:
            raise KeyError(f"Key {key} not in dof_cfg, cannot override")

        value_raw = getattr(merged_cfg, key)
        value_override_fitted = dof_adapter.fit(value_override, dim=0, template=value_raw).tolist()
        setattr(merged_cfg, key, value_override_fitted)
        logger.debug(f"[DoF] override {key} with {value_override_fitted}")
    return merged_cfg


class DoFAdapter:
    def __init__(self, src_joint_names, tar_joint_names):
        self.src_joint_names = src_joint_names
        self.tar_joint_names = tar_joint_names

        self.src_len = len(src_joint_names)
        self.tar_len = len(tar_joint_names)

        self.src_indices = []
        self.tar_indices = []

        for i, name in enumerate(src_joint_names):
            if name in tar_joint_names:
                self.src_indices.append(i)
                self.tar_indices.append(tar_joint_names.index(name))

        assert len(self.src_indices) > 0, "Error fitting src and tar joint names, please check the config."

    def fit(self, data, dim=-1, template=None) -> np.ndarray:
        if type(data) is not np.ndarray:
            data = np.asarray(data)

        assert data.shape[dim] == self.src_len, (
            f"Data shape {data.shape} does not match src length {self.src_len} at dim {dim}"
        )

        new_shape = list(data.shape)
        new_shape[dim] = self.tar_len

        if template is None:
            new_data = np.zeros(new_shape, dtype=data.dtype)
        else:
            if type(template) is not np.ndarray:
                template = np.asarray(template, dtype=data.dtype)
            new_data = template.copy()
            assert new_data.shape == tuple(new_shape), (
                f"Template shape {new_data.shape} does not match target shape {new_shape}"
            )

        if dim == -1:
            new_data[..., self.tar_indices] = data[..., self.src_indices]
        else:
            indices = [slice(None)] * len(data.shape)
            indices[dim] = self.tar_indices
            src_indices = [slice(None)] * len(data.shape)
            src_indices[dim] = self.src_indices
            new_data[tuple(indices)] = data[tuple(src_indices)]

        return new_data


if __name__ == "__main__":
    from pprint import pprint

    from robojudo.config.g1.env.g1_env_cfg import G1_29DoF
    from robojudo.config.g1.policy.g1_unitree_policy_cfg import G1UnitreeDoF
    from robojudo.tools.tool_cfgs import DoFConfig

    dof_config = merge_dof_cfgs(G1_29DoF(), G1UnitreeDoF())
    pprint(dof_config)
