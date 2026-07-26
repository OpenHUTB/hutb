import holoocean
import uuid
import pytest


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test",
        "world": "TestWorld",
        "main_agent": "turtle0",
        "lcm_provider": "memq://",
        "frames_per_sec": False,
        "octree_min": 0.1,
        "octree_max": 10,
        "env_min": [-1, -1, -1],
        "env_max": [1, 1, 1],
        "agents": [
            {
                "agent_name": "turtle0",
                "agent_type": "TurtleAgent",
                "sensors": [
                    {
                        "sensor_type": "IMUSensor",
                        "publish": "lcm",
                        "lcm_channel": "sensor",
                        "configuration": {
                            "ReturnBias": True,  # for IMU
                            "ReturnRange": True,  # for DVL
                        },
                    }
                ],
                "control_scheme": 0,
                "location": [-1.5, 1.50, 3.0],
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


@pytest.mark.parametrize(
    "sensor",
    [
        "DVLSensor",
        "IMUSensor",
        "GPSSensor",
        "ImagingSonar",
        "DepthSensor",
        "RGBCamera",
        "PoseSensor",
        "LocationSensor",
        "RangeFinderSensor",
        "RotationSensor",
        "OrientationSensor",
        "VelocitySensor",
    ],
)
def test_sensor(env, sensor):
    env._scenario["agents"][0]["sensors"][0]["sensor_type"] = sensor
    env.reset()

    d = {"i": 0}

    def my_handler(channel, data):
        d["i"] += 1

    sub = env._lcm.subscribe("sensor", my_handler)

    for _ in range(50):
        env.tick()
        env._lcm.handle()

    assert d["i"] == 50, f"LCM only received {d['i']} of 100 messages"

    env._lcm.unsubscribe(sub)
