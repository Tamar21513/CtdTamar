import asyncio
from dataclasses import dataclass


class MatchUnavailableError(RuntimeError):
    pass


@dataclass(frozen=True)
class GameServerShard:
    host: str
    port: int


class ShardPoolAllocator:
    """Reserves one of a pool of authoritative game server shards."""

    def __init__(self, shards: list[GameServerShard]) -> None:
        if not shards:
            raise ValueError(
                "At least one game server shard is required."
            )
        self._lock = asyncio.Lock()
        self._available: list[GameServerShard] = list(shards)

    async def acquire(self) -> GameServerShard:
        async with self._lock:
            if not self._available:
                raise MatchUnavailableError(
                    "All game server shards are already hosting "
                    "a match."
                )
            return self._available.pop()

    async def release(self, shard: GameServerShard) -> None:
        async with self._lock:
            self._available.append(shard)
