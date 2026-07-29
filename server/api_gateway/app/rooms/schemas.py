from typing import Any
from uuid import UUID

from app.rooms.models import ConnectedUser, GameRoom


def public_user(user: ConnectedUser) -> dict[str, str]:
    return {
        "id": str(user.user_id),
        "username": user.username,
    }


def room_created_message(room: GameRoom) -> dict[str, Any]:
    body: dict[str, Any] = {
        "room_id": str(room.room_id),
        "visibility": room.visibility,
        "status": room.status,
        "host": public_user(room.host),
    }
    if room.room_code is not None:
        body["room_code"] = room.room_code
    return {"type": "room_created", "room": body}


def game_started_message(
    room: GameRoom,
    player: ConnectedUser,
) -> dict[str, Any]:
    opponent = room.opponent(player.user_id)
    return {
        "type": "game_started",
        "room_id": str(room.room_id),
        "color": room.color_for(player.user_id),
        "opponent": public_user(opponent) if opponent else None,
    }


def watching_game_message(room: GameRoom) -> dict[str, Any]:
    return {
        "type": "watching_game",
        "room_id": str(room.room_id),
        "white": public_user(room.white) if room.white else None,
        "black": public_user(room.black) if room.black else None,
        "spectator_count": len(room.spectators),
    }


def opponent_disconnected_message(room_id: UUID) -> dict[str, str]:
    return {
        "type": "opponent_disconnected",
        "room_id": str(room_id),
    }


def room_status_message(
    room_id: UUID,
    status: str,
) -> dict[str, str]:
    return {
        "type": "room_status",
        "room_id": str(room_id),
        "status": status,
    }


def error_message(code: str, message: str) -> dict[str, str]:
    return {
        "type": "error",
        "code": code,
        "message": message,
    }
