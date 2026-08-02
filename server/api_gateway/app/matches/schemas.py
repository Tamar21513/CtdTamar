from typing import Any
from uuid import UUID


def match_ready_message(
    room_id: UUID,
    color: str,
    opponent: str,
    revision: int,
    state: dict[str, Any],
    game_starts_at: str,
    white_rating: int,
    black_rating: int,
) -> dict[str, Any]:
    return {
        "type": "match_ready",
        "room_id": str(room_id),
        "color": color,
        "opponent": opponent,
        "revision": revision,
        "state": state,
        "game_starts_at": game_starts_at,
        "white_rating": white_rating,
        "black_rating": black_rating,
    }


def match_countdown_message(
    room_id: UUID, value: int | str, game_starts_at: str
) -> dict[str, Any]:
    return {
        "type": "match_countdown",
        "room_id": str(room_id),
        "phase": "go" if value == "GO" else "countdown",
        "value": value,
        "game_starts_at": game_starts_at,
    }


def match_started_message(
    room_id: UUID,
    game_starts_at: str,
    revision: int,
    state: dict[str, Any],
) -> dict[str, Any]:
    return {
        "type": "match_started",
        "room_id": str(room_id),
        "game_starts_at": game_starts_at,
        "revision": revision,
        "state": state,
    }


def match_cancelled_message(room_id: UUID) -> dict[str, str]:
    return {
        "type": "match_cancelled",
        "room_id": str(room_id),
        "reason": "player_disconnected_before_start",
    }


def match_forfeited_message(room_id: UUID) -> dict[str, str]:
    return {
        "type": "match_forfeited",
        "room_id": str(room_id),
        "reason": "opponent_did_not_reconnect",
    }


def match_snapshot_message(
    room_id: UUID,
    revision: int,
    state: dict[str, Any],
) -> dict[str, Any]:
    return {
        "type": "match_snapshot",
        "room_id": str(room_id),
        "revision": revision,
        "state": state,
    }


def match_state_message(
    room_id: UUID,
    revision: int,
    state: dict[str, Any],
) -> dict[str, Any]:
    return {
        "type": "match_state",
        "room_id": str(room_id),
        "revision": revision,
        "state": state,
    }


def opponent_reconnecting_message(
    room_id: UUID, seconds_remaining: int
) -> dict[str, Any]:
    return {
        "type": "opponent_reconnecting",
        "room_id": str(room_id),
        "seconds_remaining": seconds_remaining,
    }


def opponent_reconnected_message(room_id: UUID) -> dict[str, str]:
    return {"type": "opponent_reconnected", "room_id": str(room_id)}


def match_resumed_message(
    room_id: UUID,
    color: str,
    opponent: str,
    revision: int,
    state: dict[str, Any],
    white_rating: int,
    black_rating: int,
) -> dict[str, Any]:
    return {
        "type": "match_resumed",
        "room_id": str(room_id),
        "color": color,
        "opponent": opponent,
        "revision": revision,
        "state": state,
        "white_rating": white_rating,
        "black_rating": black_rating,
    }


def move_result_message(
    room_id: UUID,
    sequence: int,
    accepted: bool,
    reason: str,
) -> dict[str, Any]:
    return {
        "type": "move_result",
        "room_id": str(room_id),
        "sequence": sequence,
        "accepted": accepted,
        "reason": reason,
    }
