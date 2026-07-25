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
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "sphere",
                "agent_type": "SphereAgent",
                "sensors": [{"sensor_type": "GPSSensor", "configuration": {}}],
                "control_scheme": 0,
                "location": [0, 0, 0],
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
def test_setting_depth(env, num):
    """Make sure if it's above the depth we receive data, and if below we don't"""
    depth = np.random.rand() * 10
    env._scenario["agents"][0]["sensors"][0]["configuration"]["Depth"] = depth
    env._scenario["agents"][0]["location"] = [0, 0, -1 * depth + 1]

    env.reset()

    state = env.tick()
    assert "GPSSensor" in state

    # Test below
    env._scenario["agents"][0]["location"] = [0, 0, -1 * depth - 1]

    env.reset()

    state = env.tick()
    assert "GPSSensor" not in state


@pytest.mark.parametrize("num", range(3))
def test_random_depth(env, num):
    """Make sure the depth is changing according to the noise we put in"""
    num_ticks = 100
    depth = np.random.rand() * 10
    env._scenario["agents"][0]["sensors"][0]["configuration"]["Depth"] = depth
    env._scenario["agents"][0]["sensors"][0]["configuration"]["DepthSigma"] = 1
    env._scenario["agents"][0]["location"] = [0, 0, -1 * depth]

    env.reset()

    count = 0
    for _ in range(num_ticks):
        state = env.tick()
        if "GPSSensor" in state:
            count += 1

    assert 0 < count
    assert count < num_ticks
