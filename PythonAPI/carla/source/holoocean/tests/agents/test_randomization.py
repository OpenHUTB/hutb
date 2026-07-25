import uuid
import copy
import numpy as np
import math
from holoocean import packagemanager as pm
from holoocean.environments import HoloOceanEnvironment

base_conf = {
    "name": "test_randomization",
    "world": "TestWorld",
    "package_name": "TestWorlds",
    "main_agent": "sphere0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "sphere0",
            "agent_type": "SphereAgent",
            "sensors": [
                {"sensor_type": "LocationSensor"},
                {"sensor_type": "RotationSensor"},
            ],
            "control_scheme": 0,
            "location": [0.95, -1.75, 0.5],
            "rotation": [1.0, 2.0, 3.0],
            "location_randomization": [40, 40, 40],
            "rotation_randomization": [40, 40, 40],
        }
    ],
}


def is_different_3d_vector(lhs, rhs):
    is_same = np.isclose(lhs, rhs, rtol=1e-12)
    return not is_same[0] or not is_same[1] or not is_same[2]


def check_3d_vector_variance(lhs, rhs, variance):
    first = math.isclose(lhs[0], rhs[0], rel_tol=variance[0])
    second = math.isclose(lhs[1], rhs[1], rel_tol=variance[1])
    third = math.isclose(lhs[1], rhs[1], rel_tol=variance[1])

    return first and second and third


def test_location_with_randomization():
    """
    Validate that the location of the agent is not the same between resets
    Args:

    Returns:

    """
    bin_path = pm.get_binary_path_for_package("TestWorlds")
    conf = copy.deepcopy(base_conf)

    with HoloOceanEnvironment(
        scenario=conf,
        verbose=True,
        binary_path=bin_path,
        show_viewport=False,
        uuid=str(uuid.uuid4()),
    ) as env:
        prev_location = conf["agents"][0]["location"]
        default_start_location = conf["agents"][0]["location"]
        variance = conf["agents"][0]["location_randomization"]

        num_resets = 5
        for _ in range(num_resets):
            cur_location = env.tick()["LocationSensor"]

            assert is_different_3d_vector(cur_location, prev_location)
            assert check_3d_vector_variance(
                cur_location, default_start_location, variance
            )

            prev_location = cur_location
            env.reset()


def test_rotation_with_randomization():
    """
    Validate that the rotation of the agent is not the same between resets
    Args:

    Returns:

    """
    bin_path = pm.get_binary_path_for_package("TestWorlds")
    conf = copy.deepcopy(base_conf)

    with HoloOceanEnvironment(
        scenario=conf,
        binary_path=bin_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        prev_start_rotation = conf["agents"][0]["rotation"]

        num_resets = 5
        for _ in range(num_resets):
            cur_rotation = env.tick()["RotationSensor"]

            assert is_different_3d_vector(cur_rotation, prev_start_rotation)

            prev_start_rotation = cur_rotation
            env.reset()
