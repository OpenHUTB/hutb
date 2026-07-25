"""Contains action space definitions"""
import numpy as np


class ActionSpace:
    """Abstract ActionSpace class.

    Parameters:
        shape (:obj:`list` of :obj:`int`): The shape of data that should be input to step or tick.
        buffer_shape (:obj:`list` of :obj:`int`, optional): The shape of the data that will be
            written to the shared memory.

            Only use this when it is different from shape.
    """
    def __init__(self, shape, buffer_shape=None):
        super(ActionSpace, self).__init__()
        self._shape = shape
        self.buffer_shape = buffer_shape or shape

    def sample(self):
        """Sample from the action space.

        Returns:
            (:obj:`np.ndarray`): A valid command to be input to step or tick.
        """
        raise NotImplementedError("Must be implemented by child class")

    @property
    def shape(self):
        """Get the shape of the action space.

        Returns:
            (:obj:`list` of :obj:`int`): The shape of the action space.
        """
        return self._shape

    def get_low(self):
        """The minimum value(s) for the action space.

        Returns:
            (:obj:`list` of :obj:`float` or :obj:`float`): the action space's minimum value(s)
        """
        raise NotImplementedError('Must be implemented by the child class')

    def get_high(self):
        """The maximum value(s) for the action space.

        Returns:
            (:obj:`list` of :obj:`float` or :obj:`float`): the action space's maximum value(s)
        """
        raise NotImplementedError('Must be implemented by the child class')


class ContinuousActionSpace(ActionSpace):
    """Action space that takes floating point inputs.

    Parameters:
        shape (:obj:`list` of :obj:`int`): The shape of data that should be input to step or tick.
        sample_fn (function, optional): A function that takes a shape parameter and outputs a
            sampled command.
        low (:obj:`list` of :obj:`float` or :obj:`float`): the low value(s) for the action space. Can be a scalar or an array
        high (:obj:`list` of :obj:`float` or :obj:`float`): the high value(s) for the action space. Cand be a scalar or an array

            If this is not given, it will default to sampling from a unit gaussian.
        buffer_shape (:obj:`list` of :obj:`int`, optional): The shape of the data that will be
            written to the shared memory.

            Only use this when it is different from ``shape``.
        """
    def __init__(self, shape, low=None, high=None, sample_fn=None, buffer_shape=None):
        super(ContinuousActionSpace, self).__init__(shape, buffer_shape=buffer_shape)
        self.sample_fn = sample_fn or ContinuousActionSpace._default_sample_fn
        self._low = low
        self._high = high

    def get_low(self):
        return self._low

    def get_high(self):
        return self._high

    def sample(self):
        return self.sample_fn(self._shape)

    def __repr__(self):
        return "[ContinuousActionSpace " + str(self._shape) + "]"

    @staticmethod
    def _default_sample_fn(shape):
        return np.random.normal(size=shape)


class DiscreteActionSpace(ActionSpace):
    """Action space that takes integer inputs.

    Args:
        shape (:obj:`list` of :obj:`int`): The shape of data that should be input to step or tick.
        low (:obj:`int`): The lowest value to sample.
        high (:obj:`int`): The highest value to sample.
        buffer_shape (:obj:`list` of :obj:`int`, optional): The shape of the data that will be
            written to the shared memory.

            Only use this when it is different from shape.
    """

    def __init__(self, shape, low, high, buffer_shape=None):
        super(DiscreteActionSpace, self).__init__(shape, buffer_shape=buffer_shape)
        self._low = low
        self._high = high

    def sample(self):
        return np.random.randint(self._low, self._high, self._shape, dtype=np.int32)

    def get_low(self):
        return self._low

    def get_high(self):
        return self._high

    def __repr__(self):
        return "[DiscreteActionSpace " + str(self._shape) + ", min: " +\
               str(self._low) + ", max: " + str(self._high) + "]"
