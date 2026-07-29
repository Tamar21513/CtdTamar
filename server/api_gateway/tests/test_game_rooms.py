from fastapi.testclient import TestClient

from app.auth.sessions import SESSION_PREFIX


PASSWORD = "StrongPassword123!"


def register(client: TestClient, username: str) -> dict[str, str]:
    credentials = {"username": username, "password": PASSWORD}
    response = client.post("/auth/register", json=credentials)
    assert response.status_code == 201
    return credentials


def login(
    client: TestClient,
    credentials: dict[str, str],
) -> tuple[dict[str, str], str]:
    response = client.post("/auth/login", json=credentials)
    assert response.status_code == 200
    return response.json(), response.cookies["ctd_session"]


def connect(client: TestClient):
    return client.websocket_connect("/ws")


def create_room(socket, visibility: str) -> dict:
    socket.send_json(
        {"type": "create_room", "visibility": visibility}
    )
    message = socket.receive_json()
    assert message["type"] == "room_created"
    return message["room"]


def start_public_game(
    client: TestClient,
    host_credentials: dict[str, str],
    guest_credentials: dict[str, str],
):
    host_user, _ = login(client, host_credentials)
    host_context = connect(client)
    host_socket = host_context.__enter__()
    host_socket.receive_json()
    room = create_room(host_socket, "public")

    guest_user, _ = login(client, guest_credentials)
    guest_context = connect(client)
    guest_socket = guest_context.__enter__()
    guest_socket.receive_json()
    guest_socket.send_json(
        {"type": "join_room", "room_id": room["room_id"]}
    )
    host_started = host_socket.receive_json()
    guest_started = guest_socket.receive_json()
    return (
        host_context,
        host_socket,
        host_user,
        guest_context,
        guest_socket,
        guest_user,
        room,
        host_started,
        guest_started,
    )


def test_create_public_and_hidden_rooms(
    client: TestClient,
) -> None:
    public_credentials = register(client, "public_host")
    hidden_credentials = register(client, "hidden_host")
    login(client, public_credentials)
    with connect(client) as public_socket:
        public_socket.receive_json()
        public_room = create_room(public_socket, "public")
        assert public_room["visibility"] == "public"
        assert public_room["status"] == "waiting"
        assert "room_code" not in public_room

    login(client, hidden_credentials)
    with connect(client) as hidden_socket:
        hidden_socket.receive_json()
        hidden_room = create_room(hidden_socket, "hidden")
        assert hidden_room["visibility"] == "hidden"
        assert len(hidden_room["room_code"]) == 6


def test_invalid_visibility_is_rejected(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    login(client, registered_user)
    with connect(client) as socket:
        socket.receive_json()
        socket.send_json(
            {"type": "create_room", "visibility": "private"}
        )
        assert socket.receive_json()["code"] == "invalid_visibility"


def test_public_lobby_updates_and_hidden_room_does_not_leak(
    client: TestClient,
) -> None:
    observer_credentials = register(client, "observer")
    public_credentials = register(client, "public_owner")
    hidden_credentials = register(client, "hidden_owner")
    login(client, observer_credentials)

    with connect(client) as observer:
        observer.receive_json()
        observer.send_json({"type": "subscribe_lobby"})
        initial = observer.receive_json()
        assert initial["waiting_rooms"] == []

        login(client, public_credentials)
        with connect(client) as public_host:
            public_host.receive_json()
            public_room = create_room(public_host, "public")
            update = observer.receive_json()
            assert [
                room["room_id"] for room in update["waiting_rooms"]
            ] == [public_room["room_id"]]

            login(client, hidden_credentials)
            with connect(client) as hidden_host:
                hidden_host.receive_json()
                create_room(hidden_host, "hidden")
                observer.send_json({"type": "ping"})
                assert observer.receive_json() == {"type": "pong"}


def test_public_join_starts_game_with_deterministic_colors(
    client: TestClient,
) -> None:
    host_credentials = register(client, "room_host")
    guest_credentials = register(client, "room_guest")
    values = start_public_game(
        client,
        host_credentials,
        guest_credentials,
    )
    (
        host_context,
        host_socket,
        host_user,
        guest_context,
        guest_socket,
        guest_user,
        room,
        host_started,
        guest_started,
    ) = values
    try:
        assert host_started == {
            "type": "game_started",
            "room_id": room["room_id"],
            "color": "white",
            "opponent": {
                "id": guest_user["id"],
                "username": guest_user["username"],
            },
        }
        assert guest_started == {
            "type": "game_started",
            "room_id": room["room_id"],
            "color": "black",
            "opponent": {
                "id": host_user["id"],
                "username": host_user["username"],
            },
        }
    finally:
        guest_context.__exit__(None, None, None)
        host_context.__exit__(None, None, None)


def test_hidden_join_activates_visible_game(
    client: TestClient,
) -> None:
    observer_credentials = register(client, "live_observer")
    host_credentials = register(client, "secret_host")
    guest_credentials = register(client, "secret_guest")
    login(client, observer_credentials)

    with connect(client) as observer:
        observer.receive_json()
        observer.send_json({"type": "subscribe_lobby"})
        observer.receive_json()

        host_user, _ = login(client, host_credentials)
        with connect(client) as host:
            host.receive_json()
            room = create_room(host, "hidden")
            observer.send_json({"type": "ping"})
            assert observer.receive_json() == {"type": "pong"}

            guest_user, _ = login(client, guest_credentials)
            with connect(client) as guest:
                guest.receive_json()
                guest.send_json(
                    {
                        "type": "join_hidden_room",
                        "room_code": room["room_code"].lower(),
                    }
                )
                host_started = host.receive_json()
                guest_started = guest.receive_json()
                assert host_started["color"] == "white"
                assert guest_started["color"] == "black"
                assert host_started["opponent"]["id"] == guest_user["id"]
                assert guest_started["opponent"]["id"] == host_user["id"]

                update = observer.receive_json()
                assert update["waiting_rooms"] == []
                assert update["active_games"][0]["room_id"] == (
                    room["room_id"]
                )


def test_invalid_room_rejections_and_duplicate_participation(
    client: TestClient,
) -> None:
    host_credentials = register(client, "duplicate_host")
    other_credentials = register(client, "other_host")
    login(client, host_credentials)
    with connect(client) as host:
        host.receive_json()
        room = create_room(host, "public")
        host.send_json(
            {"type": "join_room", "room_id": room["room_id"]}
        )
        assert host.receive_json()["code"] == "cannot_join_own_room"
        host.send_json(
            {"type": "create_room", "visibility": "public"}
        )
        assert host.receive_json()["code"] == "already_in_room"

        login(client, other_credentials)
        with connect(client) as other:
            other.receive_json()
            other.send_json(
                {
                    "type": "join_room",
                    "room_id": "not-a-uuid",
                }
            )
            assert other.receive_json()["code"] == "invalid_room_id"
            other.send_json(
                {
                    "type": "join_hidden_room",
                    "room_code": "BAD999",
                }
            )
            assert other.receive_json()["code"] == "invalid_room_code"


def test_full_room_cannot_be_joined_again(
    client: TestClient,
) -> None:
    host_credentials = register(client, "full_host")
    guest_credentials = register(client, "full_guest")
    third_credentials = register(client, "third_player")
    values = start_public_game(
        client,
        host_credentials,
        guest_credentials,
    )
    (
        host_context,
        _,
        _,
        guest_context,
        _,
        _,
        room,
        _,
        _,
    ) = values
    try:
        login(client, third_credentials)
        with connect(client) as third:
            third.receive_json()
            third.send_json(
                {"type": "join_room", "room_id": room["room_id"]}
            )
            assert third.receive_json()["code"] == "room_unavailable"
    finally:
        guest_context.__exit__(None, None, None)
        host_context.__exit__(None, None, None)


def test_spectator_flow_and_disconnect_count(
    client: TestClient,
) -> None:
    host_credentials = register(client, "watch_host")
    guest_credentials = register(client, "watch_guest")
    spectator_credentials = register(client, "watcher")
    observer_credentials = register(client, "count_observer")
    values = start_public_game(
        client,
        host_credentials,
        guest_credentials,
    )
    (
        host_context,
        _,
        _,
        guest_context,
        _,
        _,
        room,
        _,
        _,
    ) = values
    login(client, observer_credentials)
    observer_context = connect(client)
    observer = observer_context.__enter__()
    observer.receive_json()
    observer.send_json({"type": "subscribe_lobby"})
    initial_lobby = observer.receive_json()
    assert initial_lobby["active_games"][0]["room_id"] == room["room_id"]
    try:
        login(client, spectator_credentials)
        with connect(client) as spectator:
            spectator.receive_json()
            spectator.send_json(
                {"type": "watch_game", "room_id": room["room_id"]}
            )
            watching = spectator.receive_json()
            assert watching["type"] == "watching_game"
            assert watching["spectator_count"] == 1
            assert observer.receive_json()["active_games"][0][
                "spectator_count"
            ] == 1

            spectator.send_json(
                {"type": "watch_game", "room_id": room["room_id"]}
            )
            assert spectator.receive_json()["code"] == "already_watching"

        assert observer.receive_json()["active_games"][0][
            "spectator_count"
        ] == 0
    finally:
        observer_context.__exit__(None, None, None)
        guest_context.__exit__(None, None, None)
        host_context.__exit__(None, None, None)


def test_waiting_room_cannot_be_watched_and_player_cannot_spectate(
    client: TestClient,
) -> None:
    host_credentials = register(client, "spectate_host")
    guest_credentials = register(client, "spectate_guest")
    watcher_credentials = register(client, "early_watcher")
    login(client, host_credentials)
    with connect(client) as host:
        host.receive_json()
        room = create_room(host, "public")

        login(client, watcher_credentials)
        with connect(client) as watcher:
            watcher.receive_json()
            watcher.send_json(
                {"type": "watch_game", "room_id": room["room_id"]}
            )
            assert watcher.receive_json()["code"] == "room_not_active"

        login(client, guest_credentials)
        with connect(client) as guest:
            guest.receive_json()
            guest.send_json(
                {"type": "join_room", "room_id": room["room_id"]}
            )
            host.receive_json()
            guest.receive_json()
            guest.send_json(
                {"type": "watch_game", "room_id": room["room_id"]}
            )
            assert guest.receive_json()["code"] == (
                "player_cannot_spectate"
            )


def test_waiting_disconnect_removes_room_and_hidden_code(
    client: TestClient,
) -> None:
    host_credentials = register(client, "leaving_host")
    guest_credentials = register(client, "late_guest")
    login(client, host_credentials)
    with connect(client) as host:
        host.receive_json()
        room = create_room(host, "hidden")

    login(client, guest_credentials)
    with connect(client) as guest:
        guest.receive_json()
        guest.send_json(
            {
                "type": "join_hidden_room",
                "room_code": room["room_code"],
            }
        )
        assert guest.receive_json()["code"] == "invalid_room_code"


def test_player_disconnect_notifies_opponent_and_spectator(
    client: TestClient,
) -> None:
    host_credentials = register(client, "disconnect_host")
    guest_credentials = register(client, "disconnect_guest")
    spectator_credentials = register(client, "disconnect_watcher")
    values = start_public_game(
        client,
        host_credentials,
        guest_credentials,
    )
    (
        host_context,
        host_socket,
        _,
        guest_context,
        _,
        _,
        room,
        _,
        _,
    ) = values
    login(client, spectator_credentials)
    spectator_context = connect(client)
    spectator = spectator_context.__enter__()
    spectator.receive_json()
    spectator.send_json(
        {"type": "watch_game", "room_id": room["room_id"]}
    )
    spectator.receive_json()
    try:
        guest_context.__exit__(None, None, None)
        assert host_socket.receive_json() == {
            "type": "opponent_disconnected",
            "room_id": room["room_id"],
        }
        assert spectator.receive_json() == {
            "type": "room_status",
            "room_id": room["room_id"],
            "status": "removed",
        }
        spectator.send_json({"type": "get_lobby"})
        assert spectator.receive_json()["active_games"] == []
    finally:
        spectator_context.__exit__(None, None, None)
        host_context.__exit__(None, None, None)


def test_matchmaking_cleanup_keeps_login_session(
    client: TestClient,
) -> None:
    credentials = register(client, "session_owner")
    _, token = login(client, credentials)
    with connect(client) as socket:
        socket.receive_json()
        create_room(socket, "public")
    assert client.app.state.redis_client.exists(
        f"{SESSION_PREFIX}{token}"
    ) == 1
