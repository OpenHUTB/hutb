from robojudo.config import cfg_registry


class ConfigManager:
    def __init__(self, config_name: str, override_cfg: dict | None = None):
        self.config_name = config_name
        self.override_cfg = override_cfg

        self.cfg = self.parse_config()

    def get_cfg(self):
        return self.cfg

    def parse_config(self):
        # cfg_class = getattr(robojudo.config, self.config_name)
        cfg_class = cfg_registry.get(self.config_name)
        cfg_raw = cfg_class()
        # cfg_raw = make_g1_pipeline_cfg(
        #     env="g1_mujoco_env",
        #     policy="g1_amo_policy",
        #     ctrl=["keyboard_ctrl"],
        # )
        # cfg_class = type(cfg_raw)

        cfg = cfg_raw

        # TODO: to be deleted
        #  # override from command line or external dict
        # if self.override_cfg is not None:
        #     cfg_intp = OmegaConf.create(cfg_raw.to_dict())
        #     override_cfg = OmegaConf.create(self.override_cfg)
        #     print("Override cfg:", override_cfg)
        #     cfg_intp = OmegaConf.merge(cfg_intp, override_cfg)
        #     cfg_dict = OmegaConf.to_container(cfg_intp, resolve=True)
        #     cfg = cfg_class.model_validate(cfg_dict)  # FOR TEST

        return cfg
        # return Box(cfg_dict)


if __name__ == "__main__":
    from pprint import pprint

    config_manager = ConfigManager("G1PipelineCfg")
    cfg = config_manager.get_cfg()

    pprint(cfg)
