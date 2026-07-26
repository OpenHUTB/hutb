import holoocean
import numpy as np
import uuid
import pytest


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test_rotation_sensor",
        "world": "TestWorld",
        "main_agent": "sphere0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "sphere0",
                "agent_type": "SphereAgent",
                "sensors": [
                    {
                        "sensor_type": "RotationSensor",
                    }
                ],
                "control_scheme": 0,
                "location": [0.95, -1.75, 0.5],
            }
        ],
    }
    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
    with holoocean.environments.HoloOceanEnvironment(
        scenario=scenario,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        yield env


@pytest.mark.parametrize("num", range(3))
def test_rotation_sensor_after_teleport(env, num):
    """Make sure that the rotation sensor correctly updates after using a teleport command to
    rotate the agent

    """
    loc = [123, 3740, 1030]
    # Do much smaller rotations to avoid singularities
    rot_deg = np.random.rand(3) * 20

    env.agents["sphere0"].teleport(loc, rot_deg)
    rotation = env.tick()["RotationSensor"]

    assert np.allclose(
        rot_deg, rotation, rtol=1e-2
    ), "The agent rotated in an unexpected manner!"
