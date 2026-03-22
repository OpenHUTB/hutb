#!/usr/bin/env python

# Copyright (c) 2025 Computer Vision Center (CVC) at the Universitat Autonoma de
# Barcelona (UAB).
#
# This work is licensed under the terms of the MIT license.
# For a copy, see <https://opensource.org/licenses/MIT>.

"""
CARLA Dynamic Weather:

Connect to a CARLA Simulator instance and control the weather. Change Sun
position smoothly with time and generate storms occasionally.

With --random flag, randomly switch between weather presets at intervals.
"""

import carla

import argparse
import math
import random
import sys


def clamp(value, minimum=0.0, maximum=100.0):
    return max(minimum, min(value, maximum))


def lerp(a, b, t):
    """Linear interpolation between a and b."""
    return a + (b - a) * t


def get_weather_presets():
    """Return a list of (WeatherParameters, name) tuples for random weather mode."""
    presets = []
    # Dynamically get all available weather presets
    for name in dir(carla.WeatherParameters):
        attr = getattr(carla.WeatherParameters, name)
        # Filter out private attributes and methods, keep only weather presets
        if not name.startswith('_') and isinstance(attr, carla.WeatherParameters):
            presets.append((attr, name))
    return presets


class Sun(object):
    def __init__(self, azimuth, altitude):
        self.azimuth = azimuth
        self.altitude = altitude
        self._t = 0.0

    def tick(self, delta_seconds):
        self._t += 0.008 * delta_seconds
        self._t %= 2.0 * math.pi
        self.azimuth += 0.25 * delta_seconds
        self.azimuth %= 360.0
        self.altitude = (70 * math.sin(self._t)) - 20

    def __str__(self):
        return 'Sun(alt: %.2f, azm: %.2f)' % (self.altitude, self.azimuth)


class Storm(object):
    def __init__(self, precipitation):
        self._t = precipitation if precipitation > 0.0 else -50.0
        self._increasing = True
        self.clouds = 0.0
        self.rain = 0.0
        self.wetness = 0.0
        self.puddles = 0.0
        self.wind = 0.0
        self.fog = 0.0

    def tick(self, delta_seconds):
        delta = (1.3 if self._increasing else -1.3) * delta_seconds
        self._t = clamp(delta + self._t, -250.0, 100.0)
        self.clouds = clamp(self._t + 40.0, 0.0, 90.0)
        self.rain = clamp(self._t, 0.0, 80.0)
        delay = -10.0 if self._increasing else 90.0
        self.puddles = clamp(self._t + delay, 0.0, 85.0)
        self.wetness = clamp(self._t * 5, 0.0, 100.0)
        self.wind = 5.0 if self.clouds <= 20 else 90 if self.clouds >= 70 else 40
        self.fog = clamp(self._t - 10, 0.0, 30.0)
        if self._t == -250.0:
            self._increasing = True
        if self._t == 100.0:
            self._increasing = False

    def __str__(self):
        return 'Storm(clouds=%d%%, rain=%d%%, wind=%d%%)' % (self.clouds, self.rain, self.wind)


class RandomWeather(object):
    """Randomly switches between weather presets at specified intervals."""

    def __init__(self, world, switch_interval=30.0, transition_time=5.0):
        self.world = world
        self.presets = get_weather_presets()

        if not self.presets:
            raise RuntimeError("No weather presets found in carla.WeatherParameters")

        self.switch_interval = switch_interval  # seconds between weather changes
        self.transition_time = transition_time  # seconds for smooth transition
        self.current_preset_idx = random.randint(0, len(self.presets) - 1)
        self.target_preset_idx = self.current_preset_idx
        self.transition_elapsed = 0.0
        self.time_since_switch = 0.0
        self.in_transition = False

        # Set initial weather
        self.world.set_weather(self.presets[self.current_preset_idx][0])
        self.start_weather = self.presets[self.current_preset_idx][0]
        self.target_weather = self.start_weather

    def _interpolate_weather(self, start, target, t):
        """Interpolate between two weather states."""
        weather = carla.WeatherParameters()
        weather.cloudiness = lerp(start.cloudiness, target.cloudiness, t)
        weather.precipitation = lerp(start.precipitation, target.precipitation, t)
        weather.precipitation_deposits = lerp(start.precipitation_deposits, target.precipitation_deposits, t)
        weather.wind_intensity = lerp(start.wind_intensity, target.wind_intensity, t)
        weather.fog_density = lerp(start.fog_density, target.fog_density, t)
        weather.fog_distance = lerp(start.fog_distance, target.fog_distance, t)
        weather.fog_falloff = lerp(start.fog_falloff, target.fog_falloff, t)
        weather.wetness = lerp(start.wetness, target.wetness, t)
        weather.scattering_intensity = lerp(start.scattering_intensity, target.scattering_intensity, t)
        weather.mie_scattering_scale = lerp(start.mie_scattering_scale, target.mie_scattering_scale, t)
        weather.rayleigh_scattering_scale = lerp(start.rayleigh_scattering_scale, target.rayleigh_scattering_scale, t)
        weather.sun_azimuth_angle = lerp(start.sun_azimuth_angle, target.sun_azimuth_angle, t)
        weather.sun_altitude_angle = lerp(start.sun_altitude_angle, target.sun_altitude_angle, t)
        return weather

    def tick(self, delta_seconds):
        self.time_since_switch += delta_seconds

        # Check if it's time to switch to a new weather preset
        if not self.in_transition and self.time_since_switch >= self.switch_interval:
            self._start_new_transition()

        # Handle ongoing transition
        if self.in_transition:
            self.transition_elapsed += delta_seconds
            t = min(1.0, self.transition_elapsed / self.transition_time)

            interpolated = self._interpolate_weather(
                self.start_weather, self.target_weather, t
            )
            self.world.set_weather(interpolated)

            if t >= 1.0:
                self.in_transition = False
                self.current_preset_idx = self.target_preset_idx

    def _start_new_transition(self):
        """Start transitioning to a new random weather preset."""
        # If only one preset, no transition needed
        if len(self.presets) <= 1:
            self.time_since_switch = 0.0
            return

        # Pick a different preset than current
        new_idx = self.current_preset_idx
        while new_idx == self.current_preset_idx:
            new_idx = random.randint(0, len(self.presets) - 1)

        self.target_preset_idx = new_idx
        self.start_weather = self.world.get_weather()
        self.target_weather = self.presets[self.target_preset_idx][0]
        self.transition_elapsed = 0.0
        self.time_since_switch = 0.0
        self.in_transition = True

    def get_current_preset_name(self):
        return self.presets[self.current_preset_idx][1]

    def get_target_preset_name(self):
        if self.in_transition:
            return self.presets[self.target_preset_idx][1]
        return None

    def __str__(self):
        current = self.get_current_preset_name()
        if self.in_transition:
            target = self.get_target_preset_name()
            return f'Weather: {current} -> {target}'
        return f'Weather: {current}'


class Weather(object):
    def __init__(self, weather):
        self.weather = weather
        self._sun = Sun(weather.sun_azimuth_angle, weather.sun_altitude_angle)
        self._storm = Storm(weather.precipitation)

    def tick(self, delta_seconds):
        self._sun.tick(delta_seconds)
        self._storm.tick(delta_seconds)
        self.weather.cloudiness = self._storm.clouds
        self.weather.precipitation = self._storm.rain
        self.weather.precipitation_deposits = self._storm.puddles
        self.weather.wind_intensity = self._storm.wind
        self.weather.fog_density = self._storm.fog
        self.weather.wetness = self._storm.wetness
        self.weather.sun_azimuth_angle = self._sun.azimuth
        self.weather.sun_altitude_angle = self._sun.altitude

    def __str__(self):
        return '%s %s' % (self._sun, self._storm)


def main():
    argparser = argparse.ArgumentParser(
        description=__doc__)
    argparser.add_argument(
        '--host',
        metavar='H',
        default='127.0.0.1',
        help='IP of the host server (default: 127.0.0.1)')
    argparser.add_argument(
        '-p', '--port',
        metavar='P',
        default=2000,
        type=int,
        help='TCP port to listen to (default: 2000)')
    argparser.add_argument(
        '-s', '--speed',
        metavar='FACTOR',
        default=1.0,
        type=float,
        help='rate at which the weather changes (default: 1.0)')
    argparser.add_argument(
        '--random',
        action='store_true',
        help='enable random weather mode, randomly switch between weather presets')
    argparser.add_argument(
        '--interval',
        metavar='SECONDS',
        default=30.0,
        type=float,
        help='interval between weather changes in random mode (default: 30.0)')
    argparser.add_argument(
        '--transition',
        metavar='SECONDS',
        default=5.0,
        type=float,
        help='transition time between weather presets in random mode (default: 5.0)')
    args = argparser.parse_args()

    speed_factor = args.speed
    update_freq = 0.1 / speed_factor

    client = carla.Client(args.host, args.port)
    client.set_timeout(2.0)
    world = client.get_world()

    if args.random:
        # Random weather mode
        random_weather = RandomWeather(
            world,
            switch_interval=args.interval,
            transition_time=args.transition
        )
        print(f'Random weather mode enabled. Switching every {args.interval}s')

        elapsed_time = 0.0

        while True:
            timestamp = world.wait_for_tick(seconds=30.0).timestamp
            elapsed_time += timestamp.delta_seconds
            if elapsed_time > update_freq:
                random_weather.tick(elapsed_time)
                sys.stdout.write('\r' + str(random_weather) + 20 * ' ')
                sys.stdout.flush()
                elapsed_time = 0.0
    else:
        # Original dynamic weather mode (sun position + storm)
        weather = Weather(world.get_weather())

        elapsed_time = 0.0

        while True:
            timestamp = world.wait_for_tick(seconds=30.0).timestamp
            elapsed_time += timestamp.delta_seconds
            if elapsed_time > update_freq:
                weather.tick(speed_factor * elapsed_time)
                world.set_weather(weather.weather)
                sys.stdout.write('\r' + str(weather) + 12 * ' ')
                sys.stdout.flush()
                elapsed_time = 0.0


if __name__ == '__main__':

    main()
