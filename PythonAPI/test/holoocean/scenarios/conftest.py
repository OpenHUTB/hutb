from typing import Callable, List

import holoocean
import pytest
from holoocean import packagemanager as pm
from holoocean.environments import HoloOceanEnvironment
from holoocean.sensors import SensorDefinition


def pytest_generate_tests(metafunc):
    """Iterate over every scenario"""
    scenarios = set()
    for config, full_path in pm._iter_packages():
        for world_entry in config["worlds"]:
            for config, full_path in pm._iter_scenarios(world_entry["name"]):
                # Don't make ones with a sonar
                use = True
                name = "{}-{}".format(config["world"], config["name"])
                config = holoocean.packagemanager.get_scenario(name)
                for agent in config["agents"]:
                    for sensor in agent["sensors"]:
                        if sensor["sensor_type"] in SensorDefinition._sonar_sensors:
                            use = False
                if use:
                    scenarios.add(name)

    if "scenario" in metafunc.fixturenames:
        metafunc.parametrize("scenario", scenarios)
