from pydantic import computed_field, model_validator

from robojudo.config import Config


class ForwardKinematicCfg(Config):
    xml_path: str
    debug_viz: bool = False
    kinematic_joint_names: list[str] | None = None


class ZedOdometryCfg(Config):
    server_ip: str
    pos_offset: list[float] = [0.0, 0.0, 0.8]  # x,y,z

    zero_align: bool = True


class DoFConfig(Config):
    _subset: bool = False  # if True, simplely inheritance & pick
    _subset_joint_names: list[str] | None = None  # used when subset is True

    joint_names: list[str]
    default_pos: list[float] | None = None
    stiffness: list[float] | None = None
    damping: list[float] | None = None
    torque_limits: list[float] | None = None
    position_limits: list[list[float]] | None = None  # [[min, max], ...]

    @computed_field
    @property
    def num_dofs(self) -> int:
        return len(self.joint_names)

    @property
    def prop_keys(self) -> list[str]:
        prop_keys = list(DoFConfig.model_fields.keys())
        prop_keys_valid = []
        # remove prop that is None
        for prop_key in prop_keys:
            if getattr(self, prop_key) is not None:
                prop_keys_valid.append(prop_key)
        return prop_keys_valid

    @model_validator(mode="after")
    def check_dof_and_process_subset(self):
        length = self.num_dofs
        if self.default_pos is not None and len(self.default_pos) != length:
            raise ValueError(f"default_pos length {len(self.default_pos)} does not match num_dofs {length}")
        if self.stiffness is not None and len(self.stiffness) != length:
            raise ValueError(f"stiffness length {len(self.stiffness)} does not match num_dofs {length}")
        if self.damping is not None and len(self.damping) != length:
            raise ValueError(f"damping length {len(self.damping)} does not match num_dofs {length}")
        if self.torque_limits is not None and len(self.torque_limits) != length:
            raise ValueError(f"torque_limits length {len(self.torque_limits)} does not match num_dofs {length}")
        if self.position_limits is not None:
            if len(self.position_limits) != length:
                raise ValueError(f"position_limits length {len(self.position_limits)} does not match num_dofs {length}")
            for i, limits in enumerate(self.position_limits):
                if len(limits) != 2:
                    raise ValueError(f"position_limits[{i}] length {len(limits)} is not 2")
                if limits[0] >= limits[1]:
                    raise ValueError(f"position_limits[{i}] min {limits[0]} is not less than max {limits[1]}")

        # check subset
        if self._subset:
            if self._subset_joint_names is None or len(self._subset_joint_names) == 0:
                raise ValueError("_subset_joint_names must be provided and non-empty when _subset is True")
            for name in self._subset_joint_names:
                if name not in self.joint_names:
                    raise ValueError(f"_subset_joint_name {name} not in joint_names")

            # do the subset operation
            joint_names = self.joint_names
            subset_joint_names = self._subset_joint_names
            # print(f"[DoFConfig]processing subset joint props from {len(joint_names)} to {len(subset_joint_names)}...")
            from .dof import DoFAdapter

            dof_adapter = DoFAdapter(src_joint_names=joint_names, tar_joint_names=subset_joint_names)

            for prop_key in self.prop_keys:
                prop_val = getattr(self, prop_key)
                subset_value = dof_adapter.fit(data=prop_val, dim=0).tolist()
                assert len(subset_value) == len(subset_joint_names)
                value = subset_value
                setattr(self, prop_key, value)
        # print(f"({self.__class__.__name__}) {self.prop_keys}")
        return self
