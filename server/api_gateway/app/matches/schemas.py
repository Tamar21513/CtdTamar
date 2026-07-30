from typing import Any
from uuid import UUID


def match_ready_message(
    room_id: UUID,
    color: str,
    opponent: str,
    revision: int,
    state: dict[str, Any],
) -> dict[str, Any]:
    return {
        "type": "match_ready",
        "room_id": str(room_id),
        "color": color,
        "opponent": opponent,
        "revision": revision,
        "state": state,
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
