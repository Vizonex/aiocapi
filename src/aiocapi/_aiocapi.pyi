# Primary for Python's own exposure of Lower level types,
# They are 100% the same but the C-API part exposes it's inner
# workings
from _asyncio import Future, Task
from typing import Any

CAPI: Any
"""Main object of the aiocapi library meant to help speedup
and manipluate low level asyncio objects."""


def _is_alive() -> bool:
    """Checks if c module is successful and is working properly,
    this will always return True."""


__all__ = ("CAPI", "Future", "Task")
