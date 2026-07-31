import json

from fastapi.testclient import TestClient
from sqlalchemy import select

from app.auth.sessions import SESSION_PREFIX
from app.db.models import User


def test_login_creates_secure_ttl_session(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    response = client.post("/auth/login", json=registered_user)
    assert response.status_code == 200
    body = response.json()
    assert body["rating"] == 1200
    assert set(body) == {"id", "username", "rating", "created_at"}

    cookie = response.headers["set-cookie"].lower()
    assert "ctd_session=" in cookie
    assert "httponly" in cookie
    assert "path=/" in cookie
    assert "samesite=lax" in cookie
    assert "max-age=3600" in cookie

    keys = list(
        client.app.state.redis_client.scan_iter(
            match=f"{SESSION_PREFIX}*"
        )
    )
    assert len(keys) == 1
    ttl = client.app.state.redis_client.ttl(keys[0])
    assert 0 < ttl <= 3600
    payload = json.loads(client.app.state.redis_client.get(keys[0]))
    assert payload["user_id"] == response.json()["id"]


def test_unknown_and_wrong_password_have_same_error(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    unknown = client.post(
        "/auth/login",
        json={
            "username": "missing_user",
            "password": registered_user["password"],
        },
    )
    wrong = client.post(
        "/auth/login",
        json={
            "username": registered_user["username"],
            "password": "IncorrectPassword!",
        },
    )
    assert unknown.status_code == wrong.status_code == 401
    assert unknown.json() == wrong.json() == {
        "detail": "Invalid username or password"
    }


def test_malformed_hash_has_public_authentication_error(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    with client.app.state.db_session_factory() as session:
        user = session.scalar(select(User))
        user.password_hash = "not-an-argon2-hash"
        session.commit()
    response = client.post("/auth/login", json=registered_user)
    assert response.status_code == 401
    assert response.json() == {
        "detail": "Invalid username or password"
    }
