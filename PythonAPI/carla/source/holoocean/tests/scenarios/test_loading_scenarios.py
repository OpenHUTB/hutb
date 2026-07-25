import holoocean


def test_load_scenario(scenario):
    """Tests that every scenario can be loaded without any errors

    Also test that every agent has every sensor that is present in the config file

    TODO: We need some way of communicating with the engine to verify that the expected level was loaded.
          If the level isn't found, then Unreal just picks a default one, so we're missing that case

    Args:
        scenario (str): Scenario to test

    """
    env = holoocean.make(
        scenario, show_viewport=False, frames_per_sec=False, verbose=True
    )
    scenario = holoocean.packagemanager.get_scenario(scenario)

    # make sure it works!
    for _ in range(20):
        env.tick()

    # make sure everything is there
    assert len(env.agents) == len(scenario["agents"]), "Length of agents did not match!"

    for agent in scenario["agents"]:
        assert agent["agent_name"] in env.agents, "Agent is not in the environment!"

        assert len(agent["sensors"]) == len(
            env.agents[agent["agent_name"]].sensors
        ), "length of sensors did not match!"

        for sensor in agent["sensors"]:
            sensor_name = (
                sensor["sensor_name"]
                if "sensor_name" in sensor
                else sensor["sensor_type"]
            )
            assert (
                sensor_name in env.agents[agent["agent_name"]].sensors
            ), "Sensor is missing!"

    env.__on_exit__()
