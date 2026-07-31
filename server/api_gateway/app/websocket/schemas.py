from typing import Any


def connected_message(
    user_id: str, username: str, rating: int
) -> dict[str, Any]:
    return {
        "type": "connected",
        "user": {
            "id": user_id,
            "username": username,
            "rating": rating,
        },
    }


def error_message(code: str, message: str) -> dict[str, str]:
    return {
        "type": "error",
        "code": code,
        "message": message,
    }


def searching_message() -> dict[str, str]:
    return {"type": "searching"}


def match_not_found_message() -> dict[str, str]:
    return {"type": "match_not_found"}


def find_match_cancelled_message() -> dict[str, str]:
    return {"type": "find_match_cancelled"}
