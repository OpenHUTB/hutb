# from holoocean.environments import HoloOceanEnvironment

# def test_weather_type_scenario(env_scenario) -> None:
#     """Test that each weather type can be set in each scenario without errors.
#     If a world doesn't have the WeatherController, the engine will crash and the
#     test will fail.

#     This test only really needs to be done for each world, but since we already
#     load up every scenario, it is faster to use those already initialized
#     environments.

#     Args:
#         env_scenario (HoloOceanEnvironment, str): Tuple to test

#     """
#     env, _ = env_scenario

#     weather_types = ["sunny", "cloudy", "rain"]

#     for weather in weather_types:
#         env.weather.set_weather(weather)
#         for _ in range(10):
#             env.tick()
#         env.reset()
