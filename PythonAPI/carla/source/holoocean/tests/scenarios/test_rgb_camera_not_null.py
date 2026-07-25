# import holoocean


# def test_rgb_camera_not_null(env_scenario):
#     """Test that the RGBCamera is sending sensor data by ensuring that it is not all zeros

#     Args:
#         env_scenario ((HoloOceanEnvironment, str)): environment and scenario we are testing

#     """
#     env, scenario = env_scenario

#     # Find the names of every RGB camera and agent in the scenario
#     config = holoocean.packagemanager.get_scenario(scenario)

#     # Set of tuples of agent name to camera name
#     agent_camera_names = set()
#     for agent_cfg in config["agents"]:
#         for sensor_cfg in agent_cfg["sensors"]:
#             if sensor_cfg["sensor_type"] == holoocean.sensors.RGBCamera.sensor_type:
#                 if "sensor_name" in sensor_cfg:
#                     sensor_name = sensor_cfg["sensor_name"]
#                 else:
#                     sensor_name = sensor_cfg["sensor_type"]
#                 agent_camera_names.add((agent_cfg["agent_name"], sensor_name))

#     # Get pixel data
#     state = env.tick()
#     for agent, camera in agent_camera_names:
#         if len(env.agents) == 1:
#             pixels = state[camera]
#         else:
#             pixels = state[agent][camera]

#         # ensure that the pixels aren't all close to zero
#         assert pixels.mean() > 1
