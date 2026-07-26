"""HoloOcean Exceptions"""


class HoloOceanException(Exception):
    """Base class for a generic exception in HoloOcean.

    Args:
        message (str): The error string.
    """


class HoloOceanConfigurationException(HoloOceanException):
    """The user provided an invalid configuration for HoloOcean"""


class TimeoutException(HoloOceanException):
    """Exception raised when communicating with the engine timed out."""


class NotFoundException(HoloOceanException):
    """Raised when a package cannot be found"""

