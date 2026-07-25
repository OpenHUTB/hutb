import holoocean
import numpy as np
from tests.utils.equality import almost_equal
import uuid
from scipy.spatial.transform import Rotation
import pytest
from scipy.linalg import logm


def rot_error(A, B):
    e = logm(A @ B.T)
    return np.array([e[0, 1], e[0, 2], e[1, 2]])


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test_orientation_sensor",
        "world": "TestWorld",
        "main_agent": "sphere0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "sphere0",
                "agent_type": "SphereAgent",
                "sensors": [
                    {
                        "sensor_type": "OrientationSensor",
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
def test_orientation_sensor_after_teleport(env, num):
    """Make sure that the orientation sensor correctly updates after using a teleport command to
    rotate the agent
    """
    loc = [123, 3740, 1030]
    rot_deg = np.random.rand(3) * 45
    R = Rotation.from_euler("xyz", rot_deg, degrees=True).as_matrix()

    env.agents["sphere0"].teleport(loc, rot_deg)
    state = env.tick()
    sensed_orientation = state["OrientationSensor"]

    assert np.allclose(
        np.zeros(3), rot_error(R, sensed_orientation), atol=1e-5
    ), "Expected orientation did not match the expected orientation!"
