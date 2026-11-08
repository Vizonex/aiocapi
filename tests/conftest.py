import asyncio
from importlib import import_module
from importlib.util import find_spec

FACTORIES = {
    "asyncio": asyncio.new_event_loop,
}


def package_exists(module: str) -> None:
    """Checks to see if a package exists without needing to import or execute it's code.

    :param module: the package's name

    :returns: True if package was found, False if not.
    """
    return find_spec(module) is not None


def add_loop(module: str):
    """Adds an eventloop to the current test-suite if seen as existant"""
    if package_exists(module):
        FACTORIES[module] = import_module(module).new_event_loop


add_loop("winloop")
add_loop("uvloop")


def pytest_asyncio_loop_factories(config, item):
    return FACTORIES
