"""
GEN_CAPI
--------

Assists with creation of different C-API headers for CPython.

"""

from collections.abc import Generator
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path

from typer import Typer

app = Typer()

# basically paths back to home repo this is to ensure
# that any new generated python versions each find an
# accompanying home.
home = Path(__file__).parent.parent


# Copied from Cython but slightly modified to fit our needs
# and sanity checks.
class LinesResult:
    __slots__ = ("lines", "s")

    def __init__(self):
        self.lines: list[str] = []
        self.s: str = ""

    def put(self, s: str) -> None:
        self.s += s

    def newline(self) -> None:
        self.lines.append(self.s)
        self.s = ""

    def putline(self, s: str) -> None:
        self.put(s)
        self.newline()


class Ctx:
    """Simple context for writing C code with."""

    def __init__(
        self, python_version: "VersionInfo", result: LinesResult | None = None
    ):
        self.result = result if result is not None else LinesResult()
        self.py_ver = python_version

    def if_mac(self, macro: str):
        self.putline(f"#if {macro}")

    def define(self, macro: str) -> None:
        self.result.putline(f"#define {macro}")

    def ifndef(self, macro: str) -> None:
        self.result.putline(f"#ifndef {macro}")

    def ifdef(self, macro: str) -> None:
        self.result.putline(f"#ifdef {macro}")

    def endif(self, comment: str | None = None) -> None:
        self.result.put("#endif")
        if comment:
            self.result.putline(f" /* {comment} */")
        else:
            self.result.newline()

    def skiplines(self, lines: int) -> None:
        for _ in range(lines):
            self.newline()

    def put(self, s: str) -> None:
        self.result.put(s)

    def newline(self) -> None:
        self.result.newline()

    def putline(self, s: str) -> None:
        self.result.putline(s)

    def include(self, include: str):
        if include.startswith("<") and include.endswith(">"):
            self.result.putline(f"#include {include}")
        else:
            self.result.putline(f'#include "{include}"')

    def body_start(self):
        self.ifdef("__cplusplus")
        self.result.putline('extern "C" {')
        self.endif()

    def body_end(self):
        self.ifdef("__cplusplus")
        self.result.putline("}")
        self.endif()

    @contextmanager
    def body(self):
        self.body_start()
        yield
        self.body_end()

    def guard_start(self):
        head = self.py_ver.headerguard_repr()
        self.ifndef(head)
        self.define(head)

    def guard_end(self):
        self.endif(self.py_ver.headerguard_repr())

    @contextmanager
    def guard(self):
        self.guard_start()
        yield
        self.guard_end()

    @contextmanager
    def linespace(self):
        """make room for 2 lines in between the target."""
        self.newline()
        yield
        self.newline()


@dataclass
class VersionInfo:
    major: int
    minor: int
    micro: int = 0
    release_level: int = 0
    release_serial: int = 0

    def hex(self) -> str:
        """Writes python version macro as hexidecimal"""
        return hex(
            (self.major << 24)
            | (self.minor << 16)
            | (self.micro << 8)
            | (self.release_level << 4)
            | (self.release_serial << 0)
        )

    def human_repr(self) -> str:
        """prints version number as a human readable string"""
        return f"{self.major}.{self.minor}"

    def filename_repr(self) -> str:
        return f"aio{self.major}{self.minor}.h"

    def headerguard_repr(self) -> str:
        return f"__AIO_{self.major}{self.minor}_H__"


def parse_version(version: str):
    major, minor = version.split(".", 1)
    return VersionInfo(int(major), int(minor), 0)


@contextmanager
def header_guard(info: VersionInfo) -> Generator[Ctx, None, None]:
    ctx = Ctx(info)
    with ctx.guard():
        yield ctx


@app.command()
def raw_header(version: str):
    with header_guard(parse_version(version)) as ctx:
        with ctx.linespace():
            ctx.include("Python.h")

        # lock it to this target version.
        ctx.if_mac(
            f"(PY_MAJOR_VERSION == {ctx.py_ver.major} && PY_MINOR_VERSION == {ctx.py_ver.minor})"
        )
        ctx.skiplines(2)
        with ctx.body():
            ctx.newline()
            ctx.putline("/* PUT WHAT YOU NEED HERE */")
            ctx.newline()
        ctx.endif(f"{ctx.py_ver.human_repr()}")

    print("\n".join(ctx.result.lines))


if __name__ == "__main__":
    app()
