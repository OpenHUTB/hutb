import holoocean
import uuid
import pytest
import numpy as np


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test_torpedo",
        "world": "TestWorld",
        "main_agent": "auv0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "auv0",
                "agent_type": "TorpedoAUV",
                "sensors": [
                    {
                        "sensor_type": "DynamicsSensor",
                    }
                ],
                "control_scheme": 1,
                "location": [0, 0, -10],
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
        ticks_per_sec=30,
    ) as env:
        yield env


def test_manual_dynamics(env):
    """Test to make sure it goes to the linear and angular acceleration we set"""
    des = [1, 2, 3, 0.1, 0.2, 0.3]

    env.reset()

    state = env.step(des, 20)

    accel = state["DynamicsSensor"][0:3]
    ang_accel = state["DynamicsSensor"][9:12]

    assert np.allclose(
        des[:3], accel
    ), "Manual dynamics didn't hit the correct linear acceleration"
    assert np.allclose(
        des[3:], ang_accel
    ), "Manual dynamics didn't hit the correct angular acceleration"
