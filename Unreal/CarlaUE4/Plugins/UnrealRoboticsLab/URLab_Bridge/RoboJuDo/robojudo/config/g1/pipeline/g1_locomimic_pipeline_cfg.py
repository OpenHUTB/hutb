from robojudo.pipeline.pipeline_cfgs import RlLocoMimicPipelineCfg


class G1RlLocoMimicPipelineCfg(RlLocoMimicPipelineCfg):
    """
    Base configuration for G1 LocoMimic pipeline.
    """

    # ===== Pipeline Config =====
    robot: str = "g1"

    # ===== Upper body override Config =====
    upper_dof_num: int = 17
    # fmt: off
    upper_dof_pos_default: list[float] | None = [
        0.0, 0.0, 0.0,
        0.0, 0.3, 0.0, 1.0, 0.0, 0.0, 0.0, 
        0.0, -0.3, 0.0, 1.0, 0.0, 0.0, 0.0, 
    ]
    """Default positions of the upper body DOFs"""
    upper_dof_override_indices: list[int] | None = [
        -17, 
        -14, -13, -12, -11, -10, -9, -8, 
        -7, -6, -5, -4, -3, -2, -1
    ]
    """Indices of the upper body DOFs to be overridden, no waist roll and pitch"""
    # fmt: on
