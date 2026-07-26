import holoocean
import uuid
import pytest
import numpy as np


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test_hovering",
        "world": "TestWorld",
        "main_agent": "auv0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "auv0",
                "agent_type": "SurfaceVessel",
                "sensors": [
                    {
                        "sensor_type": "DynamicsSensor",
                    }
                ],
                "control_scheme": 0,
                "location": [10, 10, 3],
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


def test_buoyancy_from_sunk(env):
    """Make sure it floats on the surface, from initial sunk position"""
    # Going up
    env._scenario["agents"][0]["control_scheme"] = 0
    env._scenario["agents"][0]["location"] = [10, 10, -5]

    env.reset()
    state = env.tick(100)
    p = state["DynamicsSensor"][6:9]

    assert p[2] > -1, "Surface Vessel didn't float"
    assert p[2] < 1, "Surface Vessel didn't stop going up"


def test_buoyancy_from_drop(env):
    """Make sure it floats on the surface, from initial drop."""
    # Going down
    env._scenario["agents"][0]["control_scheme"] = 0
    env._scenario["agents"][0]["location"] = [10, 10, 5]

    env.reset()
    state = env.tick(100)
    p = state["DynamicsSensor"][6:9]

    assert p[2] > -1, "Surface Vessel didn't float"
    assert p[2] < 1, "Surface Vessel didn't drop"


def test_pid_controller(env):
    """Test to make sure it goes to the orientation and position we command"""
    des = [20, 20]
    env._scenario["agents"][0]["control_scheme"] = 1

    env.reset()

    state = env.step(des, 300)

    pos = state["DynamicsSensor"][6:8]

    assert np.allclose(des, pos, 1), "Controller didn't make it to the right position"


def test_manual_dynamics(env):
    """Test to make sure it goes to the linear and angular acceleration we set"""
    des = [1, 2, 3, 0.1, 0.2, 0.3]
    env._scenario["agents"][0]["control_scheme"] = 2

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
