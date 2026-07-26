from holoocean.lcm import DVLSensor, IMUSensor, GPSSensor, \
                        AcousticBeaconSensor, ImagingSonar, DepthSensor, \
                        RGBCamera, PoseSensor, LocationSensor, \
                        RangeFinderSensor, RotationSensor, OrientationSensor, \
                        VelocitySensor
import numpy as np
import os

class SensorData:
    """Wrapper class for the various types of publishable sensor data.

    Parameters:
        sensor_type (:obj:`str`): Type of sensor to be imported
        channel (:obj:`str`): Name of channel to publish to.
    """
    _sensor_keys_ = {
        "DVLSensor": DVLSensor,
        "IMUSensor": IMUSensor,
        "GPSSensor": GPSSensor,
        "AcousticBeaconSensor": AcousticBeaconSensor,
        "ImagingSonar": ImagingSonar,
        "DepthSensor": DepthSensor,
        "RGBCamera": RGBCamera,
        "PoseSensor": PoseSensor,
        "LocationSensor": LocationSensor,
        "RangeFinderSensor": RangeFinderSensor,
        "RotationSensor": RotationSensor,
        "OrientationSensor": OrientationSensor,
        "VelocitySensor": VelocitySensor,
    }

    def __init__(self, sensor_type, channel):
        self.type = sensor_type
        self.msg = self._sensor_keys_[sensor_type]()
        self.channel = channel

    def set_value(self, timestamp, value):
        """Set value in respective sensor class.

        Parameters:
            timestamp (:obj:`int`): Number of milliseconds since last data was published
            value (:obj:`list`): List of sensor data to put into LCM sensor class
            """
        self.msg.timestamp = timestamp

        if self.type == "DVLSensor":
            self.msg.velocity = value[:3].tolist()
            if value.shape[0] == 7:
                self.msg.range = value[3:].tolist()
            else:
                self.msg.range = np.full(4, np.NaN)
        elif self.type == "IMUSensor":
            self.msg.acceleration = value[0].tolist()
            self.msg.angular_velocity = value[1].tolist()
            if value.shape[0] == 4:
                self.msg.acceleration_bias = value[2].tolist()
                self.msg.angular_velocity_bias = value[3].tolist()
            else:
                self.msg.acceleration_bias = np.full(3, np.NaN)
                self.msg.angular_velocity_bias = np.full(3, np.NaN)
        elif self.type == "GPSSensor":
            self.msg.position = value.tolist()
        elif self.type == "AcousticBeaconSensor":
            self.msg.msg_type    = value[0]
            self.msg.from_beacon = value[1]
            # TODO Eventually somehow handle data passed through in value[2]
            self.msg.azimuth   = value[3] if value[0] not in ["OWAY", "MSG_REQ", "MSG_RESP"] else np.NaN
            self.msg.elevation = value[4] if value[0] not in ["OWAY", "MSG_REQ", "MSG_RESP"] else np.NaN
            self.msg.range     = value[5] if value[0] in ["MSG_RESPU", "MSG_RESPX"] else np.NaN
            self.msg.z         = value[-1] if value[0] in ["MSG_REQX", "MSG_RESPX"] else np.NaN
        elif self.type == "ImagingSonar":
            self.msg.bins_range = value.shape[0]
            self.msg.bins_azimuth = value.shape[1]
            self.msg.image = value.tolist()
        elif self.type == "DepthSensor":
            self.msg.depth = value[0]
        elif self.type == "RGBCamera":
            self.msg.height = value.shape[0]
            self.msg.width = value.shape[1]
            self.msg.channels = value.shape[2]
            self.msg.image = value.tolist()
        elif self.type == "PoseSensor":
            self.msg.matrix = value.tolist()
        elif self.type == "LocationSensor":
            self.msg.position = value.tolist()
        elif self.type == "RangeFinderSensor":
            count = len(value)
            self.msg.count = count
            self.msg.distances = value.tolist()
            self.msg.angles = np.linspace(0, 360, count, endpoint=False).tolist()
        elif self.type == "RotationSensor":
            self.msg.roll, self.msg.pitch, self.msg.yaw = value
        elif self.type == "OrientationSensor":
            self.msg.matrix = value.tolist()
        elif self.type == "VelocitySensor":
            self.msg.velocity = value.tolist()
        else:
            raise ValueError("That sensor hasn't been implemented in LCM yet")

def gen(lang, path='.', headers=None):
    """Generates LCM files for sensors in whatever language requested. 

    Args:
        lang (:obj:`str`): One of "cpp", "c", "java", "python", "lua", "csharp", "go"
        path (:obj:`str`, optional): Location to save files in. Defaults to current directory.
        headers (:obj:`str`, optional): Where to store .h files for C . Defaults to same as c files, given by path arg.
    """
    #set up all paths
    lcm_path = os.path.join( os.path.dirname(os.path.realpath(__file__)), 'sensors.lcm' )
    path = os.path.abspath(path)
    if headers is None:
        headers = path
    else:
        headers = os.path.abspath(headers)

    #determine flags to send with it
    if lang == "c":
        flags = f"-c --c-cpath {path} --c-hpath {headers}"
    elif lang == "cpp":
        flags = f"-x --cpp-hpath {path}"
    elif lang == "java":
        flags = f"-j --jpath {path}"
    elif lang == "python":
        flags = f"-p --ppath {path}"
    elif lang == "lua":
        flags = f"-l --lpath {path}"
    elif lang == "csharp":
        flags = f"--csharp --csharp-path {path}"
    elif lang == "go":
        flags = f"--go --go-path {path}"
    else:
        raise ValueError("Not a valid language for LCM files.")
    
    #run the command
    os.system(f"lcm-gen {lcm_path} {flags}")