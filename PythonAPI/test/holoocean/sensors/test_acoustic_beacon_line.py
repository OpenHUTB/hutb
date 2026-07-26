import holoocean
import uuid
import numpy as np
import pytest


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "PerfectAUV",
        "world": "TestWorld",
        "main_agent": "auv0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "auv0",
                "agent_type": "HoveringAUV",
                "sensors": [
                    {
                        "sensor_type": "AcousticBeaconSensor",
                        "location": [0, 0, 0],
                        "configuration": {"id": 0},
                    }
                ],
                "control_scheme": 0,
                "location": [0.0, 0.0, 5.0],
                "rotation": [0.0, 0.0, 0],
            },
            {
                "agent_name": "auv1",
                "agent_type": "HoveringAUV",
                "sensors": [
                    {
                        "sensor_type": "AcousticBeaconSensor",
                        "location": [0, 0, 0],
                        "configuration": {"id": 1},
                    }
                ],
                "control_scheme": 0,
                "location": [5.0, 0.0, 5.0],
                "rotation": [0.0, 0.0, 0],
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


def test_within_max_distance(env):
    "Tests to make sure that two sensors that are not within max distance do not transmit."

    env._scenario["agents"] = [
        {
            "agent_name": "auv0",
            "agent_type": "HoveringAUV",
            "sensors": [
                {
                    "sensor_type": "AcousticBeaconSensor",
                    "location": [0, 0, 0],
                    "configuration": {"MaxDistance": 1},
                }
            ],
            "control_scheme": 0,
            "location": [0.0, 0.0, 5.0],
            "rotation": [0.0, 0.0, 0],
        },
        {
            "agent_name": "auv1",
            "agent_type": "HoveringAUV",
            "sensors": [
                {
                    "sensor_type": "AcousticBeaconSensor",
                    "location": [0, 0, 0],
                    "configuration": {"MaxDistance": 1},
                }
            ],
            "control_scheme": 0,
            "location": [5.0, 0.0, 5.0],
            "rotation": [0.0, 0.0, 0],
        },
    ]

    env.reset()

    for i in range(20):
        env.send_acoustic_message(0, 1, "OWAY", "my_message")
        state = env.tick()
        assert (
            "AcousticBeaconSensor" not in state["auv1"]
        ), "Receiving beacon received data when it should not have done so."


def test_obstructed_view(env):
    """Tests to ensure that modem is unable to transmit when there is an obstruction between modems."""
    env._scenario["agents"] = [
        {
            "agent_name": "auv0",
            "agent_type": "HoveringAUV",
            "sensors": [
                {
                    "sensor_type": "AcousticBeaconSensor",
                    "configuration": {"CheckVisible": True},
                }
            ],
            "control_scheme": 1,
            "location": [-10, 10, 0.25],
            "rotation": [0, 0, 0],
        },
        {
            "agent_name": "auv1",
            "agent_type": "HoveringAUV",
            "sensors": [
                {
                    "sensor_type": "AcousticBeaconSensor",
                    "configuration": {"CheckVisible": True},
                }
            ],
            "control_scheme": 1,
            "location": [-10, 2, 0.25],
            "rotation": [0, 0, 0],
        },
    ]

    env.reset()

    for _ in range(20):
        env.send_acoustic_message(0, 1, "OWAY", "my_message")
        state = env.tick()
        assert (
            "AcousticBeaconSensor" not in state["auv1"]
        ), "Receiving beacon received data when it should not have done so."


def test_distance_noise(env):
    """Tests to ensure that noise generation for max distance is functional"""
    num_tests = 50
    tests_passed = 0

    env._scenario["agents"] = [
        {
            "agent_name": "auv0",
            "agent_type": "HoveringAUV",
            "sensors": [
                {
                    "sensor_type": "AcousticBeaconSensor",
                    "configuration": {"MaxDistance": 3, "DistanceSigma": 1},
                }
            ],
            "control_scheme": 1,
            "location": [-10, 10, 0.25],
            "rotation": [0, 0, -90],
        },
        {
            "agent_name": "auv1",
            "agent_type": "HoveringAUV",
            "sensors": [{"sensor_type": "AcousticBeaconSensor"}],
            "control_scheme": 1,
            "location": [-10, 7, 0.25],
            "rotation": [0, 0, 90],
        },
    ]

    env.reset()

    for _ in range(num_tests):
        env.send_acoustic_message(0, 1, "OWAY", "my_message")
        state = env.tick()

        if "AcousticBeaconSensor" in state["auv1"]:
            tests_passed += 1

    assert (
        tests_passed < num_tests
    ), "All messages sent when some should have failed due to noise variation."
    assert (
        tests_passed > 0
    ), "All messages failed when some should have passed due to noise variation."
