from argon2 import PasswordHasher
from fastapi.testclient import TestClient
from sqlalchemy import select

from app.db.models import User


def test_successful_registration_is_safe(client: TestClient) -> None:
    response = client.post(
        "/auth/register",
        json={
            "username": "  Player_One  ",
            "password": "StrongPassword123!",
        },
    )
    assert response.status_code == 201
    body = response.json()
    assert body["username"] == "Player_One"
    assert set(body) == {"id", "username", "created_at"}

    with client.app.state.db_session_factory() as session:
        user = session.scalar(select(User))
        assert user is not None
        assert user.username_normalized == "player_one"
        assert user.password_hash != "StrongPassword123!"
        assert PasswordHasher().verify(
            user.password_hash,
            "StrongPassword123!",
        )


def test_duplicate_username_is_case_insensitive(
    client: TestClient,
) -> None:
    first = {
        "username": "Player.One",
        "password": "StrongPassword123!",
    }
    assert client.post("/auth/register", json=first).status_code == 201
    second = {**first, "username": "player.one"}
    response = client.post("/auth/register", json=second)
    assert response.status_code == 409


def test_invalid_registration_inputs(client: TestClient) -> None:
    invalid_usernames = ["", "ab", "a" * 33, "bad name", "bad/name", "x\nname"]
    for username in invalid_usernames:
        response = client.post(
            "/auth/register",
            json={"username": username, "password": "LongEnough123!"},
        )
        assert response.status_code == 422

    for password in ["short", " " * 10, "x" * 129]:
        response = client.post(
            "/auth/register",
            json={"username": "valid_name", "password": password},
        )
        assert response.status_code == 422
