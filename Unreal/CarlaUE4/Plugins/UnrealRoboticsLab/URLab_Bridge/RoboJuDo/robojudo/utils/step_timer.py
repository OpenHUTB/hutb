import logging
from collections.abc import Callable

logger = logging.getLogger(__name__)


class StepTimer:
    """A simple step-based delayed multi-task scheduler."""

    def __init__(self):
        self._tasks: list[tuple[int, Callable[[], None]]] = []

    def add(self, func: Callable[[], None], delay_steps: int):
        """Add a delayed function to be called after delay_steps steps."""
        if delay_steps < 0:
            raise ValueError("delay_steps must be non-negative")
        self._tasks.append((delay_steps, func))

    def tick(self):
        """Advance one step, execute tasks whose timers reached zero."""
        remaining_tasks = []
        for delay, func in self._tasks:
            delay -= 1
            if delay <= 0:
                try:
                    # logger.debug(f"Executing delayed task: {func}")
                    func()
                except Exception as e:
                    import traceback

                    print(f"[StepTimer] Error in delayed task: {e}")
                    traceback.print_exc()
            else:
                remaining_tasks.append((delay, func))
        self._tasks = remaining_tasks

    def clear(self):
        """Cancel all scheduled tasks."""
        self._tasks.clear()

    def has_pending(self) -> bool:
        """Check if there are any remaining tasks."""
        return len(self._tasks) > 0


if __name__ == "__main__":
    import time

    timer = StepTimer()

    def task1():
        print("Task 1 executed")

    def task2():
        print("Task 2 executed")

    timer.add(task1, delay_steps=3)
    timer.add(task2, delay_steps=5)

    for step in range(6):
        print(f"Step {step}")
        timer.tick()
        time.sleep(0.5)
