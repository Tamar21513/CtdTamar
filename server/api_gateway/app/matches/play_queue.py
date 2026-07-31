import asyncio
import logging
from dataclasses import dataclass
from uuid import UUID

from fastapi import WebSocket

from app.rooms.models import ConnectedUser
from app.websocket.schemas import match_not_found_message


logger = logging.getLogger(__name__)

DEFAULT_TIMEOUT_SECONDS = 60.0
RATING_RANGE = 100


@dataclass
class _QueueEntry:
    user: ConnectedUser
    timeout_task: "asyncio.Task[None] | None" = None


class PlayQueue:
    """Opponent search queue for the "Play" button.

    A new arrival is paired with whichever currently-waiting entry
    is within +/-RATING_RANGE of their rating and closest to it; if
    several waiting entries are equally close, the one that has been
    waiting longest wins (entries are only ever appended to
    `_waiting`, never reordered, so a first-match-wins scan over
    `_waiting.items()` in insertion order already yields "oldest
    among ties"). If nobody waiting is in range, the new arrival
    joins the queue and waits.
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
        with immediately (removing both from the queue) if a waiting
        entry within +/-RATING_RANGE was found (closest rating wins;
        ties go to whoever has waited longest - see class docstring),
        or None if `user` is now waiting alone (a timeout task is
        scheduled to notify them if nobody arrives within the
        configured duration). Calling this again for a user who is
        already queued is a harmless no-op."""
        async with self._lock:
            if user.user_id in self._waiting:
                return None
            best_id: UUID | None = None
            best_distance: int | None = None
            for candidate_id, entry in self._waiting.items():
                distance = abs(entry.user.rating - user.rating)
                if distance <= RATING_RANGE and (
                    best_distance is None or distance < best_distance
                ):
                    best_id = candidate_id
                    best_distance = distance
            if best_id is not None:
                entry = self._waiting.pop(best_id)
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
