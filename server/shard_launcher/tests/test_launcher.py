import asyncio
import socket
import sys
from functools import wraps
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import launcher  # noqa: E402


def async_test(function):
    @wraps(function)
    def run(*args, **kwargs):
        return asyncio.run(function(*args, **kwargs))

    return run


class FakePopen:
    def __init__(self, args, **kwargs) -> None:
        self.args = args
        self.terminated = False

    def terminate(self) -> None:
        self.terminated = True

    def wait(self, timeout=None) -> int:
        return 0

    def kill(self) -> None:
        self.terminated = True


def test_pick_free_port_returns_a_bindable_port() -> None:
    port = launcher.pick_free_port()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", port))


@async_test
async def test_handle_request_spawn_success(monkeypatch) -> None:
    monkeypatch.setattr(launcher, "port_is_listening", lambda port: True)
    monkeypatch.setattr(launcher.subprocess, "Popen", FakePopen)
    shard_launcher = launcher.ShardLauncher("fake-exe-path")

    response = await launcher.handle_request(
        shard_launcher, {"action": "spawn"}
    )

    assert response["ok"] is True
    assert isinstance(response["port"], int)


@async_test
async def test_handle_request_spawn_timeout_returns_error(
    monkeypatch,
) -> None:
    monkeypatch.setattr(launcher, "port_is_listening", lambda port: False)
    monkeypatch.setattr(launcher.subprocess, "Popen", FakePopen)
    monkeypatch.setattr(launcher, "STARTUP_TIMEOUT_SECONDS", 0.05)
    monkeypatch.setattr(launcher, "STARTUP_POLL_INTERVAL_SECONDS", 0.01)
    shard_launcher = launcher.ShardLauncher("fake-exe-path")

    response = await launcher.handle_request(
        shard_launcher, {"action": "spawn"}
    )

    assert response["ok"] is False
    assert "did not start listening" in response["error"]


@async_test
async def test_handle_request_release_calls_launcher_release() -> None:
    shard_launcher = launcher.ShardLauncher("fake-exe-path")
    released_ports: list[int] = []
    shard_launcher.release = released_ports.append

    response = await launcher.handle_request(
        shard_launcher, {"action": "release", "port": 1234}
    )

    assert response == {"ok": True}
    assert released_ports == [1234]


@async_test
async def test_handle_request_unknown_action_returns_error() -> None:
    shard_launcher = launcher.ShardLauncher("fake-exe-path")

    response = await launcher.handle_request(
        shard_launcher, {"action": "bogus"}
    )

    assert response == {"ok": False, "error": "unknown_action"}


def test_release_on_unknown_port_is_a_safe_no_op() -> None:
    shard_launcher = launcher.ShardLauncher("fake-exe-path")
    shard_launcher.release(9999)
