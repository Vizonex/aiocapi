from aiocapi._aiocapi import _is_alive


def test_sanity_check() -> None:
    assert _is_alive()
