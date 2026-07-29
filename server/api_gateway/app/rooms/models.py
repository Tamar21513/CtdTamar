from dataclasses import dataclass, field
from datetime import UTC, datetime
from typing import Literal
from uuid import UUID

from fastapi import WebSocket


RoomVisibility = Literal["public", "hidden"]
RoomStatus = Literal["waiting", "active", "finished"]
PlayerColor = Literal["white", "black"]


@dataclass(frozen=True)
class ConnectedUser:
    user_id: UUID
    username: str
    websocket: WebSocket


@dataclass
class GameRoom:
    room_id: UUID
    visibility: RoomVisibility
    host: ConnectedUser
    room_code: str | None = None
    guest: ConnectedUser | None = None
    white: ConnectedUser | None = None
    black: ConnectedUser | None = None
    status: RoomStatus = "waiting"
    created_at: datetime = field(
        default_factory=lambda: datetime.now(UTC)
    )
    spectators: dict[int, ConnectedUser] = field(
        default_factory=dict
    )

    def opponent(self, user_id: UUID) -> ConnectedUser | None:
        if self.white and self.white.user_id == user_id:
            return self.black
        if self.black and self.black.user_id == user_id:
            return self.white
        return None

    def color_for(self, user_id: UUID) -> PlayerColor:
        return "white" if self.white and self.white.user_id == user_id else "black"
