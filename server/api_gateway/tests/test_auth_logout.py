from fastapi.testclient import TestClient

from app.auth.sessions import SESSION_PREFIX


def test_logout_deletes_session_and_cookie(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    login = client.post("/auth/login", json=registered_user)
    token = login.cookies["ctd_session"]
    key = f"{SESSION_PREFIX}{token}"
    assert client.app.state.redis_client.exists(key) == 1

    response = client.post("/auth/logout")
    assert response.status_code == 204
    assert client.app.state.redis_client.exists(key) == 0
    cookie = response.headers["set-cookie"].lower()
    assert "ctd_session=" in cookie
    assert "max-age=0" in cookie
    assert "path=/" in cookie
    assert "httponly" in cookie
    assert "samesite=lax" in cookie


def test_logout_is_idempotent(client: TestClient) -> None:
    assert client.post("/auth/logout").status_code == 204
    client.cookies.set("ctd_session", "invalid")
    assert client.post("/auth/logout").status_code == 204
    assert client.post("/auth/logout").status_code == 204
