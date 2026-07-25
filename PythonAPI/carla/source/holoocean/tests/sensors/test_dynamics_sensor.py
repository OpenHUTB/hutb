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
                        "sensor_type": "DynamicsSensor",
                        "socket": "ViewPort",
                        "configuration": {"UseCOM": True, "UseRPY": True},
                    },
                ],
                "control_scheme": 0,
                "location": [-1.5, -1.50, 1.0],
                "rotation": [0, 0, 0],
            },
            {
                "agent_name": "uav0",
                "agent_type": "UavAgent",
                "sensors": [
                    {
                        "sensor_type": "DynamicsSensor",
                    }
                ],
                "control_scheme": 0,
                "location": [0, 0, 20],
            },
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


def test_acceleration(env):
    """Make sure when we move forward x is positive, y is close to 0. Make sure when not moving both are close to 0"""

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        _, _, _ = env.tick()["turtle0"]["DynamicsSensor"][0:3]
    env.step([100, 0])

    # Move forward, making sure y is relatively small, and x is increasing
    for i in range(30):
        x_accel, y_accel, z_accel = env.step([100, 0])["turtle0"]["DynamicsSensor"][0:3]
        assert x_accel >= 0, f"The acceleration wasn't positive at step {i}!"
        assert y_accel <= 0.5
        assert z_accel <= 0.5

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # Move backward, making sure y is relatively small, and x is decreasing
    for i in range(30):
        x_accel, y_accel, z_accel = env.step([-100, 0])["turtle0"]["DynamicsSensor"][
            0:3
        ]
        assert x_accel <= 1e-3, f"The acceleration wasn't negative at step {i}!"
        assert y_accel <= 0.5
        assert z_accel <= 0.5

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # make sure everythign is close to 0
    x_accel, y_accel, z_accel = env.tick()["turtle0"]["DynamicsSensor"][0:3]
    assert x_accel <= 1e-2, "The x accel wasn't close enough to zero!"
    assert y_accel <= 1e-2, "The y accel wasn't close enough to zero!"


def test_angular_velocity(env):
    """Make sure when we move forward x is positive, y is close to 0. Make sure when not moving both are close to 0"""

    env.reset()

    # let it land and then start moving forward
    for _ in range(100):
        _, _, last_z_angvel = env.tick()["turtle0"]["DynamicsSensor"][9:12]
    env.step([0, 25])

    # Move forward, making sure y is relatively small, and x is increasing
    for i in range(60):
        x_angvel, y_angvel, new_z_angvel = env.step([0, 25])["turtle0"][
            "DynamicsSensor"
        ][9:12]
        assert x_angvel <= 0.5
        assert y_angvel <= 0.5
        assert new_z_angvel >= last_z_angvel, f"The angvel didn't increase at step {i}!"

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # Move backward, making sure y is relatively small, and x is decreasing
    for i in range(60):
        x_angvel, y_angvel, new_z_angvel = env.step([0, -25])["turtle0"][
            "DynamicsSensor"
        ][9:12]
        assert x_angvel <= 0.5
        assert y_angvel <= 0.5
        assert new_z_angvel <= last_z_angvel, f"The angvel didn't decrease at step {i}!"

    # Let it stop
    for _ in range(100):
        env.step([0, 0])

    # make sure everythign is close to 0
    x_angvel, y_angvel, z_angvel = env.tick()["turtle0"]["DynamicsSensor"][9:12]
    assert x_angvel <= 1e-2, "The x angvel wasn't close enough to zero!"
    assert y_angvel <= 1e-2, "The y angvel wasn't close enough to zero!"
    assert z_angvel <= 1e-2, "The z angvel wasn't close enough to zero!"


def test_velocity(env):
    """Drop the UAV, make sure the z velocity is increasingly negative as it falls.
    Make sure it zeros out after it hits the ground, and then goes positive on takeoff
    """

    env.reset()

    last_z_velocity = env.tick()["uav0"]["DynamicsSensor"][5]

    for _ in range(50):
        new_z_velocity = env.tick()["uav0"]["DynamicsSensor"][5]
        assert new_z_velocity <= last_z_velocity, "The velocity didn't decrease!"
        last_z_velocity = new_z_velocity

    # Make sure it hits the ground
    for _ in range(60):
        env.tick()

    last_z_velocity = env.tick()["uav0"]["DynamicsSensor"][5]

    assert last_z_velocity <= 1e-4, "The velocity wasn't close enough to zero!"

    # Send it flying up into the air to make sure the z velocity increases
    env.act("uav0", [0, 0, 0, 100])
    env.tick()

    # z velocity should be positive now
    for _ in range(20):
        new_z_velocity = env.tick()["uav0"]["DynamicsSensor"][5]
        assert new_z_velocity >= last_z_velocity, "The velocity didn't increase!"
        last_z_velocity = new_z_velocity


def test_params(env):
    env.reset()

    before = env.tick()["turtle0"]["DynamicsSensor"]
    assert before.shape == (18,)

    env._scenario["agents"][0]["sensors"][0]["configuration"]["UseRPY"] = False
    env._scenario["agents"][0]["sensors"][0]["configuration"]["UseCOM"] = False

    env.reset()

    after = env.tick()["turtle0"]["DynamicsSensor"]
    # Location should be different since we're not at COM
    # Should check orientation as well, but we've change how it's saved
    assert not np.allclose(after[6:9], before[6:9])
