import json
from uuid import uuid4

from fastapi.testclient import TestClient
from sqlalchemy import delete

from app.auth.sessions import SESSION_PREFIX
from app.db.models import User


def _login(
    client: TestClient,
    registered_user: dict[str, str],
) -> str:
    response = client.post("/auth/login", json=registered_user)
    assert response.status_code == 200
    return response.cookies["ctd_session"]


def test_valid_session_returns_safe_user(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    _login(client, registered_user)
    response = client.get("/auth/me")
    assert response.status_code == 200
    body = response.json()
    assert body["rating"] == 1200
    assert set(body) == {"id", "username", "rating", "created_at"}


def test_missing_empty_and_invalid_sessions_return_401(
    client: TestClient,
) -> None:
    assert client.get("/auth/me").status_code == 401
    client.cookies.set("ctd_session", "")
    assert client.get("/auth/me").status_code == 401
    client.cookies.set("ctd_session", "invalid")
    assert client.get("/auth/me").status_code == 401


def test_deleted_or_invalid_redis_session_returns_401(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    token = _login(client, registered_user)
    redis_client = client.app.state.redis_client
    redis_client.delete(f"{SESSION_PREFIX}{token}")
    assert client.get("/auth/me").status_code == 401

    redis_client.set(f"{SESSION_PREFIX}{token}", "invalid-json", ex=60)
    assert client.get("/auth/me").status_code == 401


def test_session_for_deleted_user_returns_401(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    token = _login(client, registered_user)
    with client.app.state.db_session_factory() as session:
        session.execute(delete(User))
        session.commit()
    assert client.get("/auth/me").status_code == 401

    client.app.state.redis_client.set(
        f"{SESSION_PREFIX}{token}",
        json.dumps({"user_id": str(uuid4())}),
        ex=60,
    )
    assert client.get("/auth/me").status_code == 401
