from ._aiocapi import Future, Task

__version__ = "0.1.0"

# NOTE: these are from _asynciomodule.c directly
# but we expose them for the sake of testing along
# with numerous amounts of sanity checks.
__all__ = ("Future", "Task")
