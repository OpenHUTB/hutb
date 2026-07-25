import numpy as np

import holoocean


def compare_agent_states(state1, state2, thresh=0.01, is_close=True, to_ignore=None):
    if to_ignore is None:
        to_ignore = []

    for sensor in state1:
        if sensor in to_ignore:
            continue
        close = almost_equal(state1[sensor], state2[sensor], thresh)
        if is_close != close:
            print("Sensor {} failed!".format(sensor))
            print(state1[sensor])
            print(state2[sensor])
            print("--------------------------------")
            assert is_close != close


def almost_equal(item1, item2, r_thresh=0.01, a_thresh=1e-4):
    item1 = np.array(item1).flatten()
    item2 = np.array(item2).flatten()
    if len(item1) != len(item2):
        return False
    return all(np.isclose(item1, item2, rtol=r_thresh, atol=a_thresh))


def is_full_state(state):
    return isinstance(next(iter(state.values())), dict)


# def test_main_agent_after_resetting(env_scenario):
#     """Validate that sensor data for the main agent is the same after calling .reset()

#     Args:
#         env_scenario ((HoloOceanEnvironment, str)): environment and scenario we are testing

#     """


#     env, scenario = env_scenario
#     scenario_config = holoocean.packagemanager.get_scenario(scenario)

#     main_agent = scenario_config["main_agent"]

#     test_resets = 5

#     env.reset()
#     init_state = env._get_full_state()[main_agent]
#     agent_count = len(env.agents)
#     sensor_count = sum([len(env.agents[agent].sensors) for agent in env.agents])

#     for _ in range(test_resets):
#         env.tick()
#         env.reset()
#         state = env._get_full_state()[main_agent]

#         compare_agent_states(init_state, state, 0.3, is_close=True, to_ignore=["RGBCamera", "BallLocationSensor"])
#         assert agent_count == len(env.agents)
#         assert sensor_count == sum([len(env.agents[agent].sensors) for agent in env.agents])
