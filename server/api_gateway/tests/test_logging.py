from pathlib import Path

from fastapi.testclient import TestClient

from app.config import get_settings


def _log_path() -> Path:
    return Path(get_settings().LOG_DIR) / "api_gateway.log"


def test_register_and_login_write_log_without_password(
    client: TestClient,
) -> None:
    credentials = {
        "username": "log_test_user",
        "password": "SuperSecretPassword1!",
    }

    register_response = client.post(
        "/auth/register", json=credentials
    )
    assert register_response.status_code == 201

    login_response = client.post("/auth/login", json=credentials)
    assert login_response.status_code == 200

    log_path = _log_path()
    assert log_path.exists()
    contents = log_path.read_text(encoding="utf-8")

    assert "auth_register username=log_test_user outcome=success" in contents
    assert "auth_login username=log_test_user outcome=success" in contents
    assert credentials["password"] not in contents


def test_failed_login_is_logged_without_password(
    client: TestClient,
) -> None:
    credentials = {
        "username": "log_test_user_bad",
        "password": "WrongPassword123!",
    }

    response = client.post("/auth/login", json=credentials)
    assert response.status_code == 401

    contents = _log_path().read_text(encoding="utf-8")
    assert (
        "auth_login username=log_test_user_bad "
        "outcome=invalid_credentials"
    ) in contents
    assert credentials["password"] not in contents


def test_session_token_is_never_logged(client: TestClient) -> None:
    credentials = {
        "username": "log_test_user_token",
        "password": "AnotherStrongPass1!",
    }
    client.post("/auth/register", json=credentials)
    login_response = client.post("/auth/login", json=credentials)
    assert login_response.status_code == 200

    token = login_response.cookies.get(
        get_settings().SESSION_COOKIE_NAME
    )
    assert token

    contents = _log_path().read_text(encoding="utf-8")
    assert token not in contents
