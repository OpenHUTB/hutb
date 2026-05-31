#!/usr/bin/env python3
"""CarlaAir Quick Start - Step 2: 天气控制"""
import carla
import time
import argparse

argparser = argparse.ArgumentParser(
    description=__doc__)
argparser.add_argument(
    '--host',
    metavar='H',
    default='localhost',
    help='IP of the host CARLA Simulator (default: localhost)')
argparser.add_argument(
    '-p', '--port',
    metavar='P',
    default=2000,
    type=int,
    help='TCP port of CARLA Simulator (default: 2000)')

args = argparser.parse_args()

client = carla.Client(args.host, args.port)
world = client.get_world()

presets = {
    "Sunny": {"sun_altitude_angle": 70, "cloudiness": 10},
    "Cloudy": {"sun_altitude_angle": 50, "cloudiness": 80},
    "Rainy": {"precipitation": 100, "cloudiness": 90, "wetness": 100,
              "precipitation_deposits": 80, "wind_intensity": 50},
    "Foggy": {"fog_density": 80, "fog_distance": 10, "cloudiness": 60},
    "Sunset": {"sun_altitude_angle": 5, "cloudiness": 30},
}

for name, params in presets.items():
    weather = carla.WeatherParameters()
    for k, v in params.items():
        setattr(weather, k, v)
    world.set_weather(weather)
    print(f"Weather switched to: {name}")
    time.sleep(3)

world.set_weather(carla.WeatherParameters.ClearNoon)
print("Weather has been restored to sunny.")