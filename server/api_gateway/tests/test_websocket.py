import pytest
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

from app.auth.sessions import SESSION_PREFIX
from app.websocket.authentication import AUTHENTICATION_CLOSE_CODE


def _login(
    client: TestClient,
    registered_user: dict[str, str],
) -> tuple[str, dict[str, str]]:
    response = client.post("/auth/login", json=registered_user)
    assert response.status_code == 200
    return response.cookies["ctd_session"], response.json()


def test_authenticated_connection_and_ping(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    _, expected_user = _login(client, registered_user)
    with client.websocket_connect("/ws") as websocket:
        assert websocket.receive_json() == {
            "type": "connected",
            "user": {
                "id": expected_user["id"],
                "username": expected_user["username"],
            },
        }
        websocket.send_json({"type": "ping"})
        assert websocket.receive_json() == {"type": "pong"}


def test_unauthenticated_connection_is_rejected(
    client: TestClient,
) -> None:
    with pytest.raises(WebSocketDisconnect) as error:
        with client.websocket_connect("/ws"):
            pass
    assert error.value.code == AUTHENTICATION_CLOSE_CODE


def test_invalid_session_is_rejected(client: TestClient) -> None:
    client.cookies.set("ctd_session", "invalid-session")
    with pytest.raises(WebSocketDisconnect) as error:
        with client.websocket_connect("/ws"):
            pass
    assert error.value.code == AUTHENTICATION_CLOSE_CODE


def test_expired_session_is_rejected(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    token, _ = _login(client, registered_user)
    client.app.state.redis_client.expire(
        f"{SESSION_PREFIX}{token}",
        0,
    )
    with pytest.raises(WebSocketDisconnect) as error:
        with client.websocket_connect("/ws"):
            pass
    assert error.value.code == AUTHENTICATION_CLOSE_CODE


def test_malformed_and_unsupported_messages_are_structured(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    _login(client, registered_user)
    with client.websocket_connect("/ws") as websocket:
        websocket.receive_json()
        websocket.send_text("{invalid")
        assert websocket.receive_json() == {
            "type": "error",
            "code": "invalid_json",
            "message": "Message must be valid JSON",
        }

        websocket.send_json({"type": "chess_move"})
        assert websocket.receive_json() == {
            "type": "error",
            "code": "unsupported_message_type",
            "message": "Message type is not supported",
        }


def test_disconnect_is_handled_cleanly(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    token, _ = _login(client, registered_user)
    with client.websocket_connect("/ws") as websocket:
        websocket.receive_json()
    assert client.app.state.redis_client.exists(
        f"{SESSION_PREFIX}{token}"
    ) == 1
