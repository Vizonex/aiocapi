from __future__ import annotations

from asyncio import AbstractEventLoop, get_event_loop
from typing import Any, Protocol, TypeVar, cast

import pytest
import pytest_asyncio

from aiocapi import CancelledError, Future, InvalidStateError

_T = TypeVar("_T")


class CAPI(Protocol):
    # typehints are provided for developer's sanity.
    @staticmethod
    def future_get_loop(arg: Future[Any]) -> AbstractEventLoop: ...
    @staticmethod
    def future_get_state(arg: Future[Any]) -> int: ...
    @staticmethod
    def future_set_result(arg: Future[_T], result: _T) -> None: ...
    @staticmethod
    def future_done(arg: Future[Any]) -> bool: ...
    @staticmethod
    def future_cancel(arg: Future[Any]) -> bool: ...
    @staticmethod
    def future_cancel_with_message(arg: Future[Any], message: str) -> None: ...
    @staticmethod
    def future_cancelled(arg: Future[Any]) -> bool: ...
    @staticmethod
    def future_set_exception(arg: Future[Any], exc: Exception) -> None: ...
    @staticmethod
    def future_exception(arg: Future[Any]) -> Exception | None: ...
    @staticmethod
    def future_result(arg: Future[_T]) -> _T: ...


_testcapi: CAPI = pytest.importorskip("aiocapi._testcapi")


@pytest.fixture(params=(pytest.param(_testcapi, id="C"),))
def capi(request: pytest.FixtureRequest) -> CAPI:
    return cast(CAPI, request.param)


@pytest_asyncio.fixture(scope="function")
async def fut() -> Future:
    return Future()


def check_cancelled(capi: CAPI, fut: Future[Any]) -> None:
    """multi-check to see if future was cancelled
    via multiple different functions"""
    assert capi.future_cancelled(fut)
    assert fut.cancelled()
    assert capi.future_get_state(fut) == 1
    assert fut._state == "CANCELLED"


def check_done(capi: CAPI, fut: Future[Any]) -> None:
    """multi-check to see if future was cancelled
    via multiple different functions"""
    assert capi.future_done(fut)
    assert fut.cancelled()
    # can't be pending! so anything but 0
    assert capi.future_get_state(fut) != 0
    assert fut._state == "FINISHED"


def check_pending(capi: CAPI, fut: Future[Any]) -> None:
    """multi-check to see if future was cancelled
    via multiple different functions"""
    assert capi.future_get_state(fut) == 0
    assert fut._state == "PENDING"


@pytest.mark.asyncio
class TestFuture:
    # Based off test.test_asyncio.BaseFutureTests.test_initial_state
    async def test_initial_state(self, capi: CAPI, fut: Future) -> None:
        assert capi.future_cancelled(fut) is False
        assert capi.future_done(fut) is False
        # in all cases a pending future should remain at 0.
        check_pending(capi, fut)
        capi.future_cancel(fut)
        check_cancelled(capi, fut)

    async def test_initial_state_with_pure_python_cancel(
        self, capi: CAPI, fut: Future
    ) -> None:
        assert capi.future_cancelled(fut) is False
        assert capi.future_done(fut) is False
        # in all cases a pending future should remain at 0.
        assert capi.future_get_state(fut) == 0
        fut.cancel()

    # not ready...
    @pytest.mark.xfail
    async def test_global_loop(self, capi: CAPI, fut: Future[Any]) -> None:
        assert capi.future_get_loop(fut) == get_event_loop()

    async def test_cancellation_raises(self, capi: CAPI, fut: Future[Any]) -> None:
        # Cancel, message is NULL
        capi.future_cancel(fut)
        with pytest.raises(CancelledError):
            await fut

    async def test_future_cancel_message(self, capi: CAPI, fut: Future):
        capi.future_cancel_with_message(fut, "my message")
        with pytest.raises(CancelledError):
            await fut
        assert fut._cancel_message == "my message"

    async def test_cancel(self, capi: CAPI, fut: Future[Any]):
        capi.future_cancel(fut)
        assert capi.future_done(fut)
        with pytest.raises(CancelledError):
            capi.future_result(fut)

        with pytest.raises(CancelledError):
            capi.future_exception(fut)

        with pytest.raises(InvalidStateError):
            capi.future_set_result(fut, None)
        with pytest.raises(InvalidStateError):
            capi.future_set_exception(fut, RuntimeError("this should fail."))

    async def test_exception_class(self, capi: CAPI, fut: Future[Any]) -> None:
        capi.future_set_exception(fut, RuntimeError)
        assert isinstance(fut.exception(), RuntimeError)


    async def test_yield_from_twice(self, capi: CAPI, fut: Future[int]) -> None:
        def fixture():
            yield "A"
            x = yield from fut
            yield "B", x
            y = yield from fut
            yield "C", y

        g = fixture()
        assert (next(g) == "A")  # yield 'A'.
        assert (next(g) == fut)  # First yield from f.
        capi.future_set_result(fut, 42)
        assert (next(g) == ("B", 42))  # yield 'B', x.
        # The second "yield from f" does not yield f.
        assert (next(g) == ("C", 42))  # yield 'C', y.
