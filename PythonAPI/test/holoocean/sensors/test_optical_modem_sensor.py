import holoocean
import uuid
import pytest


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test",
        "world": "TestWorld",
        "main_agent": "uav0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "uav0",
                "agent_type": "TurtleAgent",
                "sensors": [{"sensor_type": "OpticalModemSensor"}],
                "control_scheme": 1,
                "location": [-10, 10, 0.25],
                "rotation": [0, 0, -90],
            },
            {
                "agent_name": "uav1",
                "agent_type": "TurtleAgent",
                "sensors": [{"sensor_type": "OpticalModemSensor"}],
                "control_scheme": 1,
                "location": [-10, 7, 0.25],
                "rotation": [0, 0, 90],
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


def test_transmittable(env):
    env.reset()

    command = [0, 0]
    for _ in range(20):
        env.send_optical_message(0, 1, "Message")
        state = env.step(command)

        assert (
            "OpticalModemSensor" in state["uav1"]
        ), "Receiving modem did not receive data when it should have."
        assert (
            state["uav1"]["OpticalModemSensor"] == "Message"
        ), "Wrong message received."


def test_within_max_distance(env):
    "Tests to make sure that two sensors that are not within max distance do not transmit."

    env._scenario["agents"] = [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "OpticalModemSensor",
                    "configuration": {
                        "MaxDistance": 3,
                    },
                }
            ],
            "control_scheme": 1,
            "location": [-5, 10, 0.25],
            "rotation": [0, 0, -90],
        },
        {
            "agent_name": "uav1",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "OpticalModemSensor",
                    "configuration": {
                        "MaxDistance": 3,
                    },
                }
            ],
            "control_scheme": 1,
            "location": [-5, 3, 0.25],
            "rotation": [0, 0, 90],
        },
    ]

    env.reset()

    command = [0, 0, 10, 50]
    for i in range(20):
        env.send_optical_message(0, 1, "Message")
        state = env.tick()
        assert (
            "OpticalModemSensor" not in state["uav1"]
        ), "Receiving modem received data when it should not have done so."


def test_not_oriented(env):
    "Tests to make sure that two sensors that are not within max distance do not transmit."

    env._scenario["agents"] = [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [{"sensor_type": "OpticalModemSensor"}],
            "control_scheme": 1,
            "location": [-10, 10, 0.25],
            "rotation": [0, 0, 0],
        },
        {
            "agent_name": "uav1",
            "agent_type": "UavAgent",
            "sensors": [{"sensor_type": "OpticalModemSensor"}],
            "control_scheme": 1,
            "location": [-10, 7, 0.25],
            "rotation": [0, 0, -180],
        },
    ]

    env.reset()

    command = [0, 0, 10, 50]
    for i in range(20):
        env.send_optical_message(0, 1, "Message")
        state = env.step(command)
        assert (
            "OpticalModemSensor" not in state["uav1"]
        ), "Receiving modem received data when it should not have done so."


def test_obstructed_view(env):
    """Tests to ensure that modem is unable to transmit when there is an obstruction between modems."""
    env._scenario["agents"] = [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "OpticalModemSensor",
                }
            ],
            "control_scheme": 1,
            "location": [-10, 10, 0.25],
            "rotation": [0, 0, -90],
        },
        {
            "agent_name": "uav1",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "OpticalModemSensor",
                }
            ],
            "control_scheme": 1,
            "location": [-10, 2, 0.25],
            "rotation": [0, 0, -180],
        },
    ]

    env.reset()

    command = [0, 0, 10, 50]

    for _ in range(20):
        env.send_optical_message(0, 1, "Message")
        state = env.step(command)
        assert (
            "OpticalModemSensor" not in state["uav1"]
        ), "Receiving modem received data when it should not have done so."


def test_distance_noise(env):
    """Tests to ensure that noise generation for max distance is functional"""
    num_tests = 50
    tests_passed = 0

    env._scenario["agents"] = [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "OpticalModemSensor",
                    "configuration": {"MaxDistance": 3.8, "DistanceSigma": 1.5},
                }
            ],
            "control_scheme": 1,
            "location": [-10, 10, 0.25],
            "rotation": [0, 0, -90],
        },
        {
            "agent_name": "uav1",
            "agent_type": "UavAgent",
            "sensors": [{"sensor_type": "OpticalModemSensor"}],
            "control_scheme": 1,
            "location": [-10, 7, 0.25],
            "rotation": [0, 0, 90],
        },
    ]

    env.reset()

    command = [0, 0, 10, 50]
    for _ in range(num_tests):
        env.send_optical_message(0, 1, "Message")
        state = env.step(command)

        if (
            "OpticalModemSensor" in state["uav1"]
            and state["uav1"]["OpticalModemSensor"] == "Message"
        ):
            tests_passed += 1

    assert (
        tests_passed < num_tests
    ), "All messages sent when some should have failed due to noise variation."
    assert (
        tests_passed > 0
    ), "All messages failed when some should have passed due to noise variation."


def test_angle_noise(env):
    """Tests to ensure that noise generation for max distance is functional"""
    num_tests = 50
    tests_passed = 0

    env._scenario["agents"] = [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "OpticalModemSensor",
                    "configuration": {"MaxDistance": 20, "AngleSigma": 20},
                }
            ],
            "control_scheme": 1,
            "location": [-10, 10, 0.25],
            "rotation": [0, 0, -90],
        },
        {
            "agent_name": "uav1",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "OpticalModemSensor",
                    "configuration": {"MaxDistance": 20, "AngleSigma": 20},
                }
            ],
            "control_scheme": 1,
            "location": [-15.2, 7, 0.25],
            "rotation": [0, 0, 90],
        },
    ]

    env.reset()

    command = [0, 0, 10, 50]
    for _ in range(num_tests):
        env.send_optical_message(0, 1, "Message")
        state = env.step(command)
        if (
            "OpticalModemSensor" in state["uav1"]
            and state["uav1"]["OpticalModemSensor"] == "Message"
        ):
            tests_passed += 1

    assert (
        tests_passed < num_tests
    ), "All messages sent when some should have failed due to noise variation."
    assert (
        tests_passed > 0
    ), "All messages failed when some should have passed due to noise variation."
