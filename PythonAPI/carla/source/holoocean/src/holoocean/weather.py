"""Weather/time controller for environments"""
from holoocean.exceptions import HoloOceanException


class WeatherController:
    """Controller for dynamically changing weather and time in an environment

    Args:
        send_world_command (function): Callback for sending commands to a world
    """
    def __init__(self, send_world_command):
        self._send_command = send_world_command
        self.cur_weather = "sunny"

    def set_fog_density(self, density):
        """Change the fog density.

        The change will occur when :meth:`tick` or :meth:`step` is called next.

        By the next tick, the exponential height fog in the world will have the new density. If
        there is no fog in the world, it will be created with the given density.

        Args:
            density (:obj:`float`): The new density value, between 0 and 1. The command will not be
                sent if the given density is invalid.
        """
        if density < 0 or density > 1:
            raise HoloOceanException("Fog density should be between 0 and 1")

        self._send_command("SetFogDensity", num_params=[density])

    def set_day_time(self, hour):
        """Change the time of day.

        Daytime will change when :meth:`tick` or :meth:`step` is called next.

        By the next tick, the lighting and the skysphere will be updated with the new hour.

        If there is no skysphere, skylight, or directional source light in the world, this command
        will exit the environment.

        Args:
            hour (:obj:`int`): The hour in 24-hour format: [0, 23].
        """
        self._send_command("SetHour", num_params=[hour % 24])

    def start_day_cycle(self, day_length):
        """Start the day cycle.

        The cycle will start when :meth:`tick` or :meth:`step` is called next.

        The sky sphere will then update each tick with an updated sun angle as it moves about the
        sky. The length of a day will be roughly equivalent to the number of minutes given.

        If there is no skysphere, skylight, or directional source light in the world, this command
        will exit the environment.

        Args:
            day_length (:obj:`int`): The number of minutes each day will be.
        """
        if day_length <= 0:
            raise HoloOceanException("The given day length should be between above 0!")

        self._send_command("SetDayCycle", num_params=[1, day_length])

    def stop_day_cycle(self):
        """Stop the day cycle.

        The cycle will stop when :meth:`tick` or :meth:`step` is called next.

        By the next tick, day cycle will stop where it is.

        If there is no skysphere, skylight, or directional source light in the world, this command
        will exit the environment.
        """
        self._send_command("SetDayCycle", num_params=[0, -1])

    def set_weather(self, weather_type):
        """Set the world's weather.

        The new weather will be applied when :meth:`tick` or :meth:`step` is called next.

        By the next tick, the lighting, skysphere, fog, and relevant particle systems will be
        updated and/or spawned
        to the given weather.

        If there is no skysphere, skylight, or directional source light in the world, this command
        will exit the environment.

        .. note::
            Because this command can affect the fog density, any changes made by a
            ``change_fog_density`` command before a set_weather command called will be undone. It is
            recommended to call ``change_fog_density`` after calling set weather if you wish to
            apply your specific changes.

        In all downloadable worlds, the weather is sunny by default.

        If the given type string is not available, the command will not be sent.

        Args:
            weather_type (:obj:`str`): The type of weather, which can be ``rain``, ``cloudy``, or
            ``sunny``.

        """
        weather_type = weather_type.lower()
        if not weather_type in ["rain", "cloudy", "sunny"]:
            raise HoloOceanException("Invalid weather type " + weather_type)

        self.cur_weather = weather_type
        self._send_command("SetWeather", string_params=[weather_type])
