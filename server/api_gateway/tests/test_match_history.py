from uuid import uuid4

from fastapi.testclient import TestClient

from app.db.models import MatchHistory, User


PASSWORD = "StrongPassword123!"


def register(client: TestClient, username: str) -> dict[str, str]:
    credentials = {"username": username, "password": PASSWORD}
    response = client.post("/auth/register", json=credentials)
    assert response.status_code == 201
    return credentials


def login(client: TestClient, credentials: dict[str, str]) -> None:
    response = client.post("/auth/login", json=credentials)
    assert response.status_code == 200


def test_get_match_history_returns_own_perspective(
    client: TestClient,
) -> None:
    white_credentials = register(client, "history_white")
    black_credentials = register(client, "history_black")

    with client.app.state.db_session_factory() as session:
        white = session.query(User).filter_by(
            username="history_white"
        ).one()
        black = session.query(User).filter_by(
            username="history_black"
        ).one()
        session.add(
            MatchHistory(
                id=uuid4(),
                room_id=uuid4(),
                white_user_id=white.id,
                black_user_id=black.id,
                white_username="history_white",
                black_username="history_black",
                winner_color="white",
                white_rating_before=1200,
                white_rating_after=1216,
                black_rating_before=1200,
                black_rating_after=1184,
                reason="king_capture",
            )
        )
        session.commit()

    login(client, white_credentials)
    response = client.get("/matches/history")
    assert response.status_code == 200
    entries = response.json()
    assert len(entries) == 1
    entry = entries[0]
    assert entry["opponent"] == "history_black"
    assert entry["color"] == "white"
    assert entry["result"] == "win"
    assert entry["rating_before"] == 1200
    assert entry["rating_after"] == 1216
    assert entry["reason"] == "king_capture"

    login(client, black_credentials)
    response = client.get("/matches/history")
    assert response.status_code == 200
    entries = response.json()
    assert len(entries) == 1
    entry = entries[0]
    assert entry["opponent"] == "history_white"
    assert entry["color"] == "black"
    assert entry["result"] == "loss"
    assert entry["rating_before"] == 1200
    assert entry["rating_after"] == 1184


def test_get_match_history_requires_authentication(
    client: TestClient,
) -> None:
    response = client.get("/matches/history")
    assert response.status_code == 401


def test_get_match_history_empty_for_user_with_no_matches(
    client: TestClient,
) -> None:
    credentials = register(client, "history_empty")
    login(client, credentials)
    response = client.get("/matches/history")
    assert response.status_code == 200
    assert response.json() == []
