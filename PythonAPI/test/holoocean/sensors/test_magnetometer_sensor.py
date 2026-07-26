import holoocean
import uuid
import pytest
import numpy as np


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test_location_sensor",
        "world": "TestWorld",
        "main_agent": "sphere",
        "agents": [
            {
                "agent_name": "sphere",
                "agent_type": "SphereAgent",
                "sensors": [
                    {"sensor_type": "MagnetometerSensor", "configuration": {}},
                    {"sensor_type": "OrientationSensor"},
                ],
                "control_scheme": 0,
                "location": [0, 0, 0],
                "rotation": [0, 0, 0],
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


@pytest.mark.parametrize("num", range(3))
def test_setting_vector(env, num):
    """Make sure setting the vector gets us what we want"""
    vec = np.random.rand(3)
    vec /= np.linalg.norm(vec)
    env._scenario["agents"][0]["sensors"][0]["configuration"][
        "MagneticVector"
    ] = vec.tolist()
    env._scenario["agents"][0]["rotation"] = (np.random.rand(3) * 30).tolist()

    env.reset()

    state = env.tick()

    expected = state["OrientationSensor"].T @ vec

    assert np.allclose(expected, state["MagnetometerSensor"], rtol=1e-3)


@pytest.mark.parametrize("num", range(3))
def test_random(env, num):
    """Make sure enabling noise perturbs things"""
    vec = np.random.rand(3)
    vec /= np.linalg.norm(vec)
    env._scenario["agents"][0]["sensors"][0]["configuration"][
        "MagneticVector"
    ] = vec.tolist()
    env._scenario["agents"][0]["rotation"] = (np.random.rand(3) * 30).tolist()

    env._scenario["agents"][0]["sensors"][0]["configuration"]["Sigma"] = 10

    env.reset()

    state = env.tick()

    expected = state["OrientationSensor"].T @ vec

    assert not np.allclose(expected, state["MagnetometerSensor"])
