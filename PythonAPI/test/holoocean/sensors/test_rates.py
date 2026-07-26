import holoocean
import uuid

turtle_config = {
    "name": "test_velocity_sensor",
    "world": "TestWorld",
    "main_agent": "turtle0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "turtle0",
            "agent_type": "TurtleAgent",
            "sensors": [{"sensor_type": "DVLSensor", "Hz": 15}],
            "control_scheme": 0,
            "location": [-1.5, -1.50, 3.0],
        }
    ],
}


def test_rates():
    """Make sure our sensor rates are working properly"""
    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=turtle_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
        ticks_per_sec=30,
    ) as env:
        # see if it's in the first one (should be every other)
        state = env.tick()
        isin = "DVLSensor" in state

        # iterate through a bunch
        for _ in range(100):
            state = env.tick()
            new = "DVLSensor" in state
            assert isin != new
            isin = new
