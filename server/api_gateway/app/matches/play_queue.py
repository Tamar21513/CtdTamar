import asyncio
import logging
from dataclasses import dataclass
from uuid import UUID

from fastapi import WebSocket

from app.rooms.models import ConnectedUser
from app.websocket.schemas import match_not_found_message


logger = logging.getLogger(__name__)

DEFAULT_TIMEOUT_SECONDS = 60.0


@dataclass
class _QueueEntry:
    user: ConnectedUser
    timeout_task: "asyncio.Task[None] | None" = None


class PlayQueue:
    """FIFO opponent search queue for the "Play" button.

    No rating filtering here - any two waiting players are paired
    immediately in arrival order. Rating-aware (+/-100) pairing is a
    later stage; this queue only decides *when* two players are both
    waiting, not whether they are a good match.
    """

    def __init__(
        self,
        timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    ) -> None:
        self._lock = asyncio.Lock()
        self._waiting: dict[UUID, _QueueEntry] = {}
        self._timeout_seconds = timeout_seconds

    @staticmethod
    async def _send(websocket: WebSocket, message: dict) -> None:
        try:
            await websocket.send_json(message)
        except RuntimeError:
            pass

    async def find_match(
        self,
        user: ConnectedUser,
    ) -> ConnectedUser | None:
        """Adds `user` to the queue. Returns the opponent to pair
        with immediately (removing both from the queue) if one was
        already waiting, or None if `user` is now waiting alone (a
        timeout task is scheduled to notify them if nobody arrives
        within the configured duration). Calling this again for a
        user who is already queued is a harmless no-op."""
        async with self._lock:
            if user.user_id in self._waiting:
                return None
            if self._waiting:
                _, entry = next(iter(self._waiting.items()))
                del self._waiting[entry.user.user_id]
                if entry.timeout_task is not None:
                    entry.timeout_task.cancel()
                return entry.user
            new_entry = _QueueEntry(user)
            new_entry.timeout_task = asyncio.create_task(
                self._expire(user.user_id)
            )
            self._waiting[user.user_id] = new_entry
            return None

    async def _expire(self, user_id: UUID) -> None:
        await asyncio.sleep(self._timeout_seconds)
        async with self._lock:
            entry = self._waiting.pop(user_id, None)
        if entry is not None:
            logger.info("find_match_timeout user_id=%s", user_id)
            await self._send(
                entry.user.websocket, match_not_found_message()
            )

    async def leave(self, user_id: UUID) -> bool:
        """Removes `user_id` from the queue if present (voluntary
        cancel or disconnect cleanup). Returns whether they were
        actually waiting."""
        async with self._lock:
            entry = self._waiting.pop(user_id, None)
        if entry is None:
            return False
        if entry.timeout_task is not None:
            entry.timeout_task.cancel()
        return True
