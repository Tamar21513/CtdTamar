import asyncio
import json
from dataclasses import dataclass


class MatchUnavailableError(RuntimeError):
    pass


@dataclass(frozen=True)
class GameServerShard:
    host: str
    port: int


class ShardLauncherClient:
    """Talks to the standalone shard launcher over a small TCP protocol."""

    def __init__(self, host: str, port: int) -> None:
        self._host = host
        self._port = port

    async def spawn(self) -> GameServerShard:
        reader, writer = await asyncio.open_connection(
            self._host, self._port
        )
        try:
            writer.write(
                json.dumps({"action": "spawn"}).encode() + b"\n"
            )
            await writer.drain()
            raw = await asyncio.wait_for(
                reader.readline(), timeout=15.0
            )
            response = json.loads(raw)
        finally:
            writer.close()
        if not response.get("ok"):
            raise MatchUnavailableError(
                response.get(
                    "error",
                    "Shard launcher failed to start a new shard.",
                )
            )
        return GameServerShard(self._host, response["port"])

    async def release(self, shard: "GameServerShard") -> None:
        reader, writer = await asyncio.open_connection(
            self._host, self._port
        )
        try:
            writer.write(
                json.dumps(
                    {"action": "release", "port": shard.port}
                ).encode() + b"\n"
            )
            await writer.drain()
            await asyncio.wait_for(reader.readline(), timeout=5.0)
        finally:
            writer.close()


class ShardPoolAllocator:
    """Reserves a shard from a static pool, or spawns one dynamically."""

    def __init__(
        self,
        shards: list[GameServerShard],
        launcher: "ShardLauncherClient | None" = None,
        max_dynamic_shards: int = 50,
    ) -> None:
        if not shards:
            raise ValueError(
                "At least one game server shard is required."
            )
        self._lock = asyncio.Lock()
        self._available: list[GameServerShard] = list(shards)
        self._launcher = launcher
        self._max_dynamic_shards = max_dynamic_shards
        self._dynamic_count = 0
        self._dynamic_shards: set[GameServerShard] = set()

    async def acquire(self) -> GameServerShard:
        async with self._lock:
            if self._available:
                return self._available.pop()
            if (
                self._launcher is not None
                and self._dynamic_count < self._max_dynamic_shards
            ):
                self._dynamic_count += 1
            else:
                raise MatchUnavailableError(
                    "All game server shards are already hosting "
                    "a match."
                )
        try:
            shard = await self._launcher.spawn()
        except Exception:
            async with self._lock:
                self._dynamic_count -= 1
            raise
        async with self._lock:
            self._dynamic_shards.add(shard)
        return shard

    async def release(self, shard: GameServerShard) -> None:
        async with self._lock:
            is_dynamic = shard in self._dynamic_shards
            if is_dynamic:
                self._dynamic_shards.discard(shard)
        if is_dynamic:
            assert self._launcher is not None
            await self._launcher.release(shard)
            async with self._lock:
                self._dynamic_count -= 1
        else:
            async with self._lock:
                self._available.append(shard)
