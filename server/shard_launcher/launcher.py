import asyncio
import json
import os
import socket
import subprocess
import sys


DEFAULT_LISTEN_PORT = 5100
STARTUP_TIMEOUT_SECONDS = 10.0
STARTUP_POLL_INTERVAL_SECONDS = 0.2


def pick_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def port_is_listening(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.settimeout(0.2)
        try:
            probe.connect(("127.0.0.1", port))
            return True
        except OSError:
            return False


class ShardLauncher:
    """Spawns and kills independent ctd_server.exe processes on request."""

    def __init__(self, exe_path: str) -> None:
        self._exe_path = exe_path
        self._processes: dict[int, subprocess.Popen] = {}

    async def spawn(self) -> int:
        port = pick_free_port()
        process = subprocess.Popen([self._exe_path, str(port)])
        self._processes[port] = process
        loop = asyncio.get_event_loop()
        deadline = loop.time() + STARTUP_TIMEOUT_SECONDS
        while loop.time() < deadline:
            if port_is_listening(port):
                return port
            await asyncio.sleep(STARTUP_POLL_INTERVAL_SECONDS)
        process.terminate()
        del self._processes[port]
        raise RuntimeError(
            f"ctd_server.exe did not start listening on port {port} "
            "in time"
        )

    def release(self, port: int) -> None:
        process = self._processes.pop(port, None)
        if process is None:
            return
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()

    def shutdown(self) -> None:
        for port in list(self._processes):
            self.release(port)


async def handle_request(
    launcher: ShardLauncher, request: dict
) -> dict:
    action = request.get("action")
    if action == "spawn":
        try:
            port = await launcher.spawn()
            return {"ok": True, "port": port}
        except RuntimeError as error:
            return {"ok": False, "error": str(error)}
    if action == "release":
        launcher.release(int(request.get("port", -1)))
        return {"ok": True}
    return {"ok": False, "error": "unknown_action"}


async def _handle_connection(
    launcher: ShardLauncher,
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
) -> None:
    try:
        raw = await reader.readline()
        if not raw:
            return
        request = json.loads(raw)
        response = await handle_request(launcher, request)
        writer.write((json.dumps(response) + "\n").encode())
        await writer.drain()
    finally:
        writer.close()


async def main() -> None:
    listen_port = int(
        os.environ.get(
            "CTD_SHARD_LAUNCHER_PORT", str(DEFAULT_LISTEN_PORT)
        )
    )
    exe_path = os.environ.get(
        "CTD_SERVER_EXE_PATH",
        r"build\server\chess_server\Release\ctd_server.exe",
    )
    launcher = ShardLauncher(exe_path)
    server = await asyncio.start_server(
        lambda r, w: _handle_connection(launcher, r, w),
        "0.0.0.0",
        listen_port,
    )
    print(
        f"Shard launcher listening on 0.0.0.0:{listen_port}, "
        f"exe={exe_path}"
    )
    try:
        async with server:
            await server.serve_forever()
    finally:
        launcher.shutdown()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
