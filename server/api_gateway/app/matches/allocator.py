import asyncio


class MatchUnavailableError(RuntimeError):
    pass


class SingleMatchAllocator:
    """Reserves the one authoritative server supported in Stage 3G."""

    def __init__(self) -> None:
        self._lock = asyncio.Lock()
        self._allocated = False

    async def acquire(self) -> None:
        async with self._lock:
            if self._allocated:
                raise MatchUnavailableError(
                    "The game server is already hosting a match."
                )
            self._allocated = True

    async def release(self) -> None:
        async with self._lock:
            self._allocated = False
