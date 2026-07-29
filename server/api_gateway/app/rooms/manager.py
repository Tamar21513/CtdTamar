import asyncio
import secrets
from dataclasses import dataclass, field
from typing import Any
from uuid import UUID, uuid4

from fastapi import WebSocket

from app.rooms.models import (
    ConnectedUser,
    GameRoom,
    RoomVisibility,
)
from app.rooms.schemas import (
    opponent_disconnected_message,
    public_user,
    room_status_message,
)


ROOM_CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
ROOM_CODE_LENGTH = 6


class RoomOperationError(RuntimeError):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.message = message


@dataclass(frozen=True)
class CleanupNotification:
    websocket: WebSocket
    message: dict[str, Any]


@dataclass
class CleanupResult:
    lobby_changed: bool = False
    notifications: list[CleanupNotification] = field(
        default_factory=list
    )


class RoomManager:
    def __init__(self) -> None:
        self._lock = asyncio.Lock()
        self._rooms: dict[UUID, GameRoom] = {}
        self._hidden_codes: dict[str, UUID] = {}
        self._player_room_by_user: dict[UUID, UUID] = {}
        self._spectator_room_by_connection: dict[int, UUID] = {}
        self._lobby_subscribers: dict[int, WebSocket] = {}

    def _new_room_code_locked(self) -> str:
        while True:
            code = "".join(
                secrets.choice(ROOM_CODE_ALPHABET)
                for _ in range(ROOM_CODE_LENGTH)
            )
            if code not in self._hidden_codes:
                return code

    def _snapshot_locked(self) -> dict[str, Any]:
        waiting_rooms = [
            {
                "room_id": str(room.room_id),
                "host": public_user(room.host),
                "visibility": room.visibility,
                "status": room.status,
                "created_at": room.created_at.isoformat(),
            }
            for room in self._rooms.values()
            if room.status == "waiting"
            and room.visibility == "public"
        ]
        active_games = [
            {
                "room_id": str(room.room_id),
                "white": public_user(room.white),
                "black": public_user(room.black),
                "status": room.status,
                "spectator_count": len(room.spectators),
                "created_at": room.created_at.isoformat(),
            }
            for room in self._rooms.values()
            if room.status == "active"
            and room.white is not None
            and room.black is not None
        ]
        waiting_rooms.sort(key=lambda item: item["created_at"])
        active_games.sort(key=lambda item: item["created_at"])
        return {
            "type": "lobby_snapshot",
            "waiting_rooms": waiting_rooms,
            "active_games": active_games,
        }

    async def create_room(
        self,
        user_id: UUID,
        username: str,
        websocket: WebSocket,
        visibility: str,
    ) -> GameRoom:
        if visibility not in {"public", "hidden"}:
            raise RoomOperationError(
                "invalid_visibility",
                "Visibility must be public or hidden.",
            )
        async with self._lock:
            if user_id in self._player_room_by_user:
                raise RoomOperationError(
                    "already_in_room",
                    "User is already participating in a room.",
                )
            room_code = (
                self._new_room_code_locked()
                if visibility == "hidden"
                else None
            )
            room = GameRoom(
                room_id=uuid4(),
                visibility=visibility,
                host=ConnectedUser(user_id, username, websocket),
                room_code=room_code,
            )
            self._rooms[room.room_id] = room
            self._player_room_by_user[user_id] = room.room_id
            if room_code:
                self._hidden_codes[room_code] = room.room_id
            return room

    def _join_locked(
        self,
        room: GameRoom,
        user_id: UUID,
        username: str,
        websocket: WebSocket,
    ) -> GameRoom:
        if room.host.user_id == user_id:
            raise RoomOperationError(
                "cannot_join_own_room",
                "The host cannot join their own room.",
            )
        if user_id in self._player_room_by_user:
            raise RoomOperationError(
                "already_in_room",
                "User is already participating in a room.",
            )
        if room.status != "waiting" or room.guest is not None:
            raise RoomOperationError(
                "room_unavailable",
                "Room is no longer available.",
            )
        guest = ConnectedUser(user_id, username, websocket)
        room.guest = guest
        room.white = room.host
        room.black = guest
        room.status = "active"
        self._player_room_by_user[user_id] = room.room_id
        if room.room_code:
            self._hidden_codes.pop(room.room_code, None)
        return room

    async def join_public_room(
        self,
        room_id: UUID,
        user_id: UUID,
        username: str,
        websocket: WebSocket,
    ) -> GameRoom:
        async with self._lock:
            room = self._rooms.get(room_id)
            if room is None:
                raise RoomOperationError(
                    "room_not_found",
                    "Room was not found.",
                )
            if room.visibility != "public":
                raise RoomOperationError(
                    "room_not_public",
                    "Room is not public.",
                )
            return self._join_locked(
                room,
                user_id,
                username,
                websocket,
            )

    async def join_hidden_room(
        self,
        room_code: str,
        user_id: UUID,
        username: str,
        websocket: WebSocket,
    ) -> GameRoom:
        normalized_code = room_code.strip().upper()
        async with self._lock:
            room_id = self._hidden_codes.get(normalized_code)
            room = self._rooms.get(room_id) if room_id else None
            if room is None:
                raise RoomOperationError(
                    "invalid_room_code",
                    "Room code is invalid or no longer available.",
                )
            return self._join_locked(
                room,
                user_id,
                username,
                websocket,
            )

    async def watch_room(
        self,
        room_id: UUID,
        user_id: UUID,
        username: str,
        websocket: WebSocket,
    ) -> GameRoom:
        connection_id = id(websocket)
        async with self._lock:
            room = self._rooms.get(room_id)
            if room is None:
                raise RoomOperationError(
                    "room_not_found",
                    "Room was not found.",
                )
            if room.status != "active":
                raise RoomOperationError(
                    "room_not_active",
                    "Only active games can be watched.",
                )
            if (
                room.white and room.white.user_id == user_id
            ) or (
                room.black and room.black.user_id == user_id
            ):
                raise RoomOperationError(
                    "player_cannot_spectate",
                    "Players cannot spectate their own game.",
                )
            if connection_id in self._spectator_room_by_connection:
                raise RoomOperationError(
                    "already_watching",
                    "Connection is already watching a game.",
                )
            room.spectators[connection_id] = ConnectedUser(
                user_id,
                username,
                websocket,
            )
            self._spectator_room_by_connection[connection_id] = room_id
            return room

    async def lobby_snapshot(self) -> dict[str, Any]:
        async with self._lock:
            return self._snapshot_locked()

    async def subscribe_lobby(
        self,
        websocket: WebSocket,
    ) -> dict[str, Any]:
        connection_id = id(websocket)
        async with self._lock:
            if connection_id in self._lobby_subscribers:
                raise RoomOperationError(
                    "already_subscribed",
                    "Connection is already subscribed to the lobby.",
                )
            self._lobby_subscribers[connection_id] = websocket
            return self._snapshot_locked()

    async def lobby_delivery(
        self,
    ) -> tuple[list[WebSocket], dict[str, Any]]:
        async with self._lock:
            return (
                list(self._lobby_subscribers.values()),
                self._snapshot_locked(),
            )

    async def remove_connection(
        self,
        user_id: UUID,
        websocket: WebSocket,
    ) -> CleanupResult:
        connection_id = id(websocket)
        async with self._lock:
            self._lobby_subscribers.pop(connection_id, None)
            result = CleanupResult()

            spectator_room_id = self._spectator_room_by_connection.pop(
                connection_id,
                None,
            )
            if spectator_room_id is not None:
                spectator_room = self._rooms.get(spectator_room_id)
                if spectator_room is not None:
                    spectator_room.spectators.pop(connection_id, None)
                    result.lobby_changed = True

            room_id = self._player_room_by_user.get(user_id)
            room = self._rooms.get(room_id) if room_id else None
            if room is None:
                return result

            owns_connection = (
                room.host.user_id == user_id
                and room.host.websocket is websocket
            ) or (
                room.guest is not None
                and room.guest.user_id == user_id
                and room.guest.websocket is websocket
            )
            if not owns_connection:
                return result

            self._rooms.pop(room.room_id, None)
            if room.room_code:
                self._hidden_codes.pop(room.room_code, None)
            self._player_room_by_user.pop(room.host.user_id, None)
            if room.guest:
                self._player_room_by_user.pop(
                    room.guest.user_id,
                    None,
                )
            for spectator_id in room.spectators:
                self._spectator_room_by_connection.pop(
                    spectator_id,
                    None,
                )

            if room.status == "active":
                opponent = room.opponent(user_id)
                if opponent:
                    result.notifications.append(
                        CleanupNotification(
                            opponent.websocket,
                            opponent_disconnected_message(room.room_id),
                        )
                    )
                status_message = room_status_message(
                    room.room_id,
                    "removed",
                )
                result.notifications.extend(
                    CleanupNotification(
                        spectator.websocket,
                        status_message,
                    )
                    for spectator in room.spectators.values()
                )
                result.lobby_changed = True
            elif room.visibility == "public":
                result.lobby_changed = True
            return result
