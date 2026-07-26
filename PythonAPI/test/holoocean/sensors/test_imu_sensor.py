import holoocean
import uuid
import pytest

import numpy as np


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test_imu_sensor",
        "world": "TestWorld",
        "main_agent": "turtle0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "turtle0",
                "agent_type": "TurtleAgent",
                "sensors": [
                    {
                        "sensor_type": "IMUSensor",
                    },
                    {
                        "sensor_type": "IMUSensor",
                        "sensor_name": "noise",
                        "configuration": {
                            "AccelSigma": 10,
                            "AngVelSigma": 10,
                            "ReturnBias": True,
                        },
                    },
                ],
                "control_scheme": 0,
                "location": [-1.5, -1.50, 1.0],
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


def test_imu_sensor_acceleration(env):
    """Make sure when we move forward x is positive, y is close to 0. Make sure when not moving both are close to 0"""

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        _, _, _ = env.tick()["IMUSensor"][0]
    env.step([100, 0])

    # Move forward, making sure y is relatively small, and x is increasing
    for i in range(30):
        x_accel, y_accel, z_accel = env.step([100, 0])["IMUSensor"][0]
        assert x_accel >= 0, f"The acceleration wasn't positive at step {i}!"
        assert y_accel <= 0.5
        assert np.isclose(z_accel, 9.81, 1e-2), f"The z_accel wasn't 9.81 at step {i}!"

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # Move backward, making sure y is relatively small, and x is decreasing
    for i in range(30):
        x_accel, y_accel, z_accel = env.step([-100, 0])["IMUSensor"][0]
        assert x_accel <= 1e-3, f"The acceleration wasn't negative at step {i}!"
        assert y_accel <= 0.5
        assert np.isclose(z_accel, 9.81, 1e-2)

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # make sure everythign is close to 0
    x_accel, y_accel, z_accel = env.tick()["IMUSensor"][0]
    assert x_accel <= 1e-2, "The x accel wasn't close enough to zero!"
    assert y_accel <= 1e-2, "The y accel wasn't close enough to zero!"


def test_imu_sensor_angular_velocity(env):
    """Make sure when we move forward x is positive, y is close to 0. Make sure when not moving both are close to 0"""

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        _, _, last_z_angvel = env.tick()["IMUSensor"][1]
    env.step([0, 25])

    # Move forward, making sure y is relatively small, and x is increasing
    for i in range(60):
        x_angvel, y_angvel, new_z_angvel = env.step([0, 25])["IMUSensor"][1]
        assert x_angvel <= 0.5
        assert y_angvel <= 0.5
        assert new_z_angvel >= last_z_angvel, f"The angvel didn't increase at step {i}!"

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # Move backward, making sure y is relatively small, and x is decreasing
    for i in range(60):
        x_angvel, y_angvel, new_z_angvel = env.step([0, -25])["IMUSensor"][1]
        assert x_angvel <= 0.5
        assert y_angvel <= 0.5
        assert new_z_angvel <= last_z_angvel, f"The angvel didn't decrease at step {i}!"

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # make sure everythign is close to 0
    x_angvel, y_angvel, z_angvel = env.tick()["IMUSensor"][1]
    assert x_angvel <= 1e-2, "The x angvel wasn't close enough to zero!"
    assert y_angvel <= 1e-2, "The y angvel wasn't close enough to zero!"
    assert z_angvel <= 1e-2, "The z angvel wasn't close enough to zero!"


def test_imu_noise_accel(env):
    """Make sure turning on noise actually turns it on"""

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        state = env.tick()
        assert not np.allclose(state["IMUSensor"][0], state["noise"][0])


def test_imu_noise_angvel(env):
    """Make sure turning on noise actually turns it on"""

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        state = env.tick()
        assert not np.allclose(state["IMUSensor"][1], state["noise"][1])


def test_imu_noise_returnbias(env):
    """Make sure returning the bias actually works"""

    env.reset()

    # let it land and then start moving forward
    for _ in range(10):
        state = env.tick()
        assert state["IMUSensor"].shape == (2, 3)
        assert state["noise"].shape == (4, 3)
        assert np.allclose(np.zeros(3), state["noise"][2])
        assert np.allclose(np.zeros(3), state["noise"][3])


def test_imu_noise_bias_accel(env):
    """Make sure turning on noise actually turns it on"""

    env._scenario["agents"][0]["sensors"][1]["configuration"]["AccelBiasSigma"] = 10

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        state = env.tick()
        assert not np.allclose(np.zeros(3), state["noise"][2])


def test_imu_noise_bias_angvel(env):
    """Make sure turning on noise actually turns it on"""

    env._scenario["agents"][0]["sensors"][1]["configuration"]["AngVelBiasSigma"] = 10

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        state = env.tick()
        assert not np.allclose(np.zeros(3), state["noise"][3])
