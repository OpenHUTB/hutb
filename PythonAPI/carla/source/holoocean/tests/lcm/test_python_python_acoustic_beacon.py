import holoocean
from holoocean.lcm import AcousticBeaconSensor
import uuid
import pytest
import numpy as np


@pytest.fixture(scope="module")
def env():
    scenario = {
        "name": "test",
        "world": "TestWorld",
        "main_agent": "turtle0",
        "lcm_provider": "memq://",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "turtle0",
                "agent_type": "TurtleAgent",
                "sensors": [
                    {
                        "sensor_type": "AcousticBeaconSensor",
                        "sensor_name": "Zero",
                        "location": [0, 0, 0],
                        "configuration": {"id": 0},
                    },
                    {
                        "sensor_type": "AcousticBeaconSensor",
                        "sensor_name": "One",
                        "location": [1, 0, 0],
                        "configuration": {"id": 1},
                        "lcm_channel": "sensor",
                    },
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


def test_acoustic_beacon(env):
    d = {"i": 0}

    def my_handler(channel, data):
        d["i"] += 1

    env.reset()

    sub = env._lcm.subscribe("sensor", my_handler)

    for _ in range(100):
        env.send_acoustic_message(0, 1, "OWAY", None)
        env.tick()
        env._lcm.handle()

    assert d["i"] == 100, f"LCM only received {d['i']} of 100 messages"

    env._lcm.unsubscribe(sub)


@pytest.mark.parametrize(
    "msg",
    [
        "OWAY",
        "OWAYU",
        "MSG_REQU",
        "MSG_RESPU",
        "MSG_REQ",
        "MSG_RESP",
        "MSG_REQX",
        "MSG_RESPX",
    ],
)
def test_acoustic_types(msg, env):
    def my_handler(channel, data):
        temp = AcousticBeaconSensor.decode(data)

        assert temp.from_beacon == 0
        if temp.msg_type == "OWAY":
            assert np.isnan(temp.azimuth)
            assert np.isnan(temp.elevation)
            assert np.isnan(temp.range)
            assert np.isnan(temp.z)
        elif temp.msg_type == "OWAYU":
            assert ~np.isnan(temp.azimuth)
            assert ~np.isnan(temp.elevation)
            assert np.isnan(temp.range)
            assert np.isnan(temp.z)
        elif temp.msg_type == "MSG_REQ":
            assert np.isnan(temp.azimuth)
            assert np.isnan(temp.elevation)
            assert np.isnan(temp.range)
            assert np.isnan(temp.z)
        elif temp.msg_type == "MSG_RESP":
            assert np.isnan(temp.azimuth)
            assert np.isnan(temp.elevation)
            assert np.isnan(temp.range)
            assert np.isnan(temp.z)
        elif temp.msg_type == "MSG_REQU":
            assert ~np.isnan(temp.azimuth)
            assert ~np.isnan(temp.elevation)
            assert np.isnan(temp.range)
            assert np.isnan(temp.z)
        elif temp.msg_type == "MSG_RESPU":
            assert ~np.isnan(temp.azimuth)
            assert ~np.isnan(temp.elevation)
            assert ~np.isnan(temp.range)
            assert np.isnan(temp.z)
        elif temp.msg_type == "MSG_REQX":
            assert ~np.isnan(temp.azimuth)
            assert ~np.isnan(temp.elevation)
            assert np.isnan(temp.range)
            assert ~np.isnan(temp.z)
        elif temp.msg_type == "MSG_RESPX":
            assert ~np.isnan(temp.azimuth)
            assert ~np.isnan(temp.elevation)
            assert ~np.isnan(temp.range)
            assert ~np.isnan(temp.z)

    env.reset()

    sub = env._lcm.subscribe("sensor", my_handler)

    # Only send every other tick so we don't interfere with returning messages
    for i in range(100):
        if i % 2 == 0:
            env.send_acoustic_message(0, 1, msg, None)
        env.tick()
        if i % 2 == 1:
            env._lcm.handle()

    env._lcm.unsubscribe(sub)
