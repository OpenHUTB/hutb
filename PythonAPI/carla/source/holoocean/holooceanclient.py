"""The client used for subscribing shared memory between python and c++."""
import os

from holoocean.exceptions import HoloOceanException
from holoocean.shmem import Shmem


class HoloOceanClient:
    """HoloOceanClient for controlling a shared memory session.

    Args:
        uuid (:obj:`str`, optional): A UUID to indicate which server this client is associated with.
            The same UUID should be passed to the world through a command line flag. Defaults to "".
    """

    def __init__(self, uuid=""):
        self._uuid = uuid

        # Important functions
        self._get_semaphore_fn = None
        self._release_semaphore_fn = None
        self._semaphore1 = None
        self._semaphore2 = None
        self.unlink = None
        self.command_center = None

        self._memory = dict()
        self._sensors = dict()
        self._agents = dict()
        self._settings = dict()

        if os.name == "nt":
            self.__windows_init__()
        elif os.name == "posix":
            self.__posix_init__()
        else:
            raise HoloOceanException("Currently unsupported os: " + os.name)

    def __windows_init__(self):
        import win32event

        semaphore_all_access = 0x1F0003

        self._semaphore1 = win32event.OpenSemaphore(
            semaphore_all_access,
            False,
            "Global\\HOLODECK_SEMAPHORE_SERVER" + self._uuid,
        )
        self._semaphore2 = win32event.OpenSemaphore(
            semaphore_all_access,
            False,
            "Global\\HOLODECK_SEMAPHORE_CLIENT" + self._uuid,
        )

        def windows_acquire_semaphore(sem, timeout):
            result = win32event.WaitForSingleObject(sem, timeout * 1000)

            if result != win32event.WAIT_OBJECT_0:
                raise TimeoutError("Timed out or error waiting for engine!")

        def windows_release_semaphore(sem):
            win32event.ReleaseSemaphore(sem, 1)

        def windows_unlink():
            pass

        self._get_semaphore_fn = windows_acquire_semaphore
        self._release_semaphore_fn = windows_release_semaphore
        self.unlink = windows_unlink

    def __posix_init__(self):
        import posix_ipc

        self._semaphore1 = posix_ipc.Semaphore(
            "/HOLODECK_SEMAPHORE_SERVER" + self._uuid
        )
        self._semaphore2 = posix_ipc.Semaphore(
            "/HOLODECK_SEMAPHORE_CLIENT" + self._uuid
        )

        # Unfortunately, OSX doesn't support sem_timedwait(), so setting this timeout
        # does nothing.
        def posix_acquire_semaphore(sem, timeout):
            sem.acquire(timeout)

        def posix_release_semaphore(sem):
            sem.release()

        def posix_unlink():
            posix_ipc.unlink_semaphore(self._semaphore1.name)
            posix_ipc.unlink_semaphore(self._semaphore2.name)
            for shmem_block in self._memory.values():
                shmem_block.unlink()

        self._get_semaphore_fn = posix_acquire_semaphore
        self._release_semaphore_fn = posix_release_semaphore
        self.unlink = posix_unlink

    def acquire(self, timeout=60):
        """Used to acquire control. Will wait until the HolodeckServer has finished its work."""
        self._get_semaphore_fn(self._semaphore2, timeout)

    def release(self):
        """Used to release control. Will allow the HolodeckServer to take a step."""
        self._release_semaphore_fn(self._semaphore1)

    def malloc(self, key, shape, dtype):
        """Allocates a block of shared memory, and returns a numpy array whose data corresponds
        with that block.

        Args:
            key (:obj:`str`): The key to identify the block.
            shape (:obj:`list` of :obj:`int`): The shape of the numpy array to allocate.
            dtype (type): The numpy data type (e.g. np.float32).

        Returns:
            :obj:`np.ndarray`: The numpy array that is positioned on the shared memory.
        """
        if (
            key not in self._memory
            or self._memory[key].shape != shape
            or self._memory[key].dtype != dtype
        ):
            self._memory[key] = Shmem(key, shape, dtype, self._uuid)

        return self._memory[key].np_array
