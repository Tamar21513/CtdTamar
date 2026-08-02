from fastapi.testclient import TestClient
from sqlalchemy import select

from app.auth.sessions import SESSION_PREFIX
from app.db.models import User
from app.matches.play_queue import PlayQueue


PASSWORD = "StrongPassword123!"


def register(client: TestClient, username: str) -> dict[str, str]:
    credentials = {"username": username, "password": PASSWORD}
    response = client.post("/auth/register", json=credentials)
    assert response.status_code == 201
    return credentials


def set_rating(client: TestClient, username: str, rating: int) -> None:
    with client.app.state.db_session_factory() as session:
        user = session.scalar(
            select(User).where(User.username == username)
        )
        user.rating = rating
        session.commit()


def login(
    client: TestClient,
    credentials: dict[str, str],
) -> tuple[dict[str, str], str]:
    response = client.post("/auth/login", json=credentials)
    assert response.status_code == 200
    return response.json(), response.cookies["ctd_session"]


def connect(client: TestClient):
    return client.websocket_connect("/ws")


def create_room(
    socket,
    visibility: str,
    name: str | None = None,
) -> dict:
    room_name = name or f"{visibility.title()} Room {id(socket)}"
    socket.send_json(
        {
            "type": "create_room",
            "name": room_name,
            "visibility": visibility,
        }
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
    while host_socket.receive_json()["type"] != "match_started":
        pass
    while guest_socket.receive_json()["type"] != "match_started":
        pass
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
        public_room = create_room(
            public_socket, "public", "Public Arena"
        )
        assert public_room["name"] == "Public Arena"
        assert public_room["visibility"] == "public"
        assert public_room["status"] == "waiting"
        assert "room_code" not in public_room

    login(client, hidden_credentials)
    with connect(client) as hidden_socket:
        hidden_socket.receive_json()
        hidden_room = create_room(
            hidden_socket, "hidden", "Private Match"
        )
        assert hidden_room["name"] == "Private Match"
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
            {
                "type": "create_room",
                "name": "Invalid Visibility",
                "visibility": "private",
            }
        )
        assert socket.receive_json()["code"] == "invalid_visibility"


def test_room_name_validation_and_trimming(
    client: TestClient,
    registered_user: dict[str, str],
) -> None:
    login(client, registered_user)
    with connect(client) as socket:
        socket.receive_json()
        for invalid_name in ("", "   ", "é" * 21):
            socket.send_json(
                {
                    "type": "create_room",
                    "name": invalid_name,
                    "visibility": "public",
                }
            )
            error = socket.receive_json()
            assert error["type"] == "error"
            assert error["code"] == "invalid_room_name"

        room = create_room(
            socket, "public", "  Trimmed Room  "
        )
        assert room["name"] == "Trimmed Room"


def test_room_names_are_unique_for_waiting_and_active_rooms(
    client: TestClient,
) -> None:
    host_credentials = register(client, "name_host")
    guest_credentials = register(client, "name_guest")
    other_credentials = register(client, "name_other")
    login(client, host_credentials)
    with connect(client) as host:
        host.receive_json()
        room = create_room(host, "public", "Unique Arena")

        login(client, other_credentials)
        with connect(client) as other:
            other.receive_json()
            other.send_json(
                {
                    "type": "create_room",
                    "name": "unique arena",
                    "visibility": "hidden",
                }
            )
            error = other.receive_json()
            assert error == {
                "type": "error",
                "code": "room_name_taken",
                "message": "A room with this name already exists.",
            }

            login(client, guest_credentials)
            with connect(client) as guest:
                guest.receive_json()
                guest.send_json(
                    {
                        "type": "join_room",
                        "room_id": room["room_id"],
                    }
                )
                host.receive_json()
                guest.receive_json()

                other.send_json(
                    {
                        "type": "create_room",
                        "name": "UNIQUE ARENA",
                        "visibility": "public",
                    }
                )
                assert other.receive_json()["code"] == (
                    "room_name_taken"
                )


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
            assert update["waiting_rooms"][0]["name"] == (
                public_room["name"]
            )
            assert [
                room["room_id"] for room in update["waiting_rooms"]
            ] == [public_room["room_id"]]

            login(client, hidden_credentials)
            with connect(client) as hidden_host:
                hidden_host.receive_json()
                hidden_room = create_room(
                    hidden_host, "hidden", "Secret Name"
                )
                observer.send_json({"type": "ping"})
                assert observer.receive_json() == {"type": "pong"}
                observer.send_json({"type": "get_lobby"})
                snapshot = observer.receive_json()
                assert all(
                    room.get("name") != hidden_room["name"]
                    for room in snapshot["waiting_rooms"]
                )


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
        assert host_started["type"] == "match_ready"
        assert guest_started["type"] == "match_ready"
        assert host_started["room_id"] == room["room_id"]
        assert guest_started["room_id"] == room["room_id"]
        assert host_started["color"] == "white"
        assert guest_started["color"] == "black"
        assert host_started["opponent"] == guest_user["username"]
        assert guest_started["opponent"] == host_user["username"]
        assert host_started["revision"] == 1
        assert host_started["state"] == guest_started["state"]
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
                assert host_started["opponent"] == guest_user["username"]
                assert guest_started["opponent"] == host_user["username"]

                update = observer.receive_json()
                assert update["waiting_rooms"] == []
                assert update["active_games"][0]["room_id"] == (
                    room["room_id"]
                )
                assert update["active_games"][0]["name"] == (
                    room["name"]
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
            {
                "type": "create_room",
                "name": "Another Room",
                "visibility": "public",
            }
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
            assert watching["name"] == room["name"]
            assert watching["spectator_count"] == 1
            snapshot = spectator.receive_json()
            assert snapshot["type"] == "match_snapshot"
            assert snapshot["revision"] == 1
            assert observer.receive_json()["active_games"][0][
                "spectator_count"
            ] == 1

            spectator.send_json(
                {"type": "watch_game", "room_id": room["room_id"]}
            )
            assert spectator.receive_json()["code"] == "already_watching"
            spectator.send_json(
                {
                    "type": "leave_spectator",
                    "room_id": room["room_id"],
                }
            )
            assert spectator.receive_json() == {
                "type": "spectator_left",
                "room_id": room["room_id"],
            }
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
            response = guest.receive_json()
            while response.get("type") == "match_countdown":
                response = guest.receive_json()
            assert response["code"] == "player_cannot_spectate"


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
    spectator.receive_json()
    try:
        guest_context.__exit__(None, None, None)
        # A mid-match disconnect now starts a grace period at the
        # MatchManager level (see manager.cleanup_room), which pushes
        # this reconnect countdown to the opponent before the
        # room-level opponent_disconnected notification below - the
        # room itself is still removed from the lobby immediately,
        # unaffected by the match-level grace period.
        assert host_socket.receive_json() == {
            "type": "opponent_reconnecting",
            "room_id": room["room_id"],
            "seconds_remaining": 20,
        }
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


def test_connected_and_room_payloads_include_rating(
    client: TestClient,
) -> None:
    credentials = register(client, "rating_display_user")
    login(client, credentials)
    with connect(client) as socket:
        connected = socket.receive_json()
        assert connected["type"] == "connected"
        assert connected["user"]["rating"] == 1200
        room = create_room(socket, "public", "Rating Room")
        assert room["host"]["rating"] == 1200


def test_room_create_uses_fresh_rating_not_connect_time_snapshot(
    client: TestClient,
) -> None:
    credentials = register(client, "midconn_rating_user")
    login(client, credentials)
    with connect(client) as socket:
        connected = socket.receive_json()
        assert connected["user"]["rating"] == 1200
        # Simulate a rating change (e.g. from a match elsewhere)
        # happening after this websocket connected but before it
        # takes its next lobby action - the room must reflect the
        # DB's current value, not the rating cached at connect time.
        set_rating(client, "midconn_rating_user", 1450)
        room = create_room(socket, "public", "Fresh Rating Room")
        assert room["host"]["rating"] == 1450


def test_find_match_pairs_two_waiting_players(
    client: TestClient,
) -> None:
    first_credentials = register(client, "queue_first")
    second_credentials = register(client, "queue_second")

    login(client, first_credentials)
    with connect(client) as first_socket:
        first_socket.receive_json()
        first_socket.send_json({"type": "find_match"})
        assert first_socket.receive_json() == {"type": "searching"}

        login(client, second_credentials)
        with connect(client) as second_socket:
            second_socket.receive_json()
            second_socket.send_json({"type": "find_match"})

            first_ready = first_socket.receive_json()
            second_ready = second_socket.receive_json()
            assert first_ready["type"] == "match_ready"
            assert second_ready["type"] == "match_ready"
            assert first_ready["color"] == "white"
            assert second_ready["color"] == "black"
            assert (
                first_ready["room_id"] == second_ready["room_id"]
            )


def test_find_match_alone_receives_searching_ack(
    client: TestClient,
) -> None:
    credentials = register(client, "lone_seeker")
    login(client, credentials)
    with connect(client) as socket:
        socket.receive_json()
        socket.send_json({"type": "find_match"})
        assert socket.receive_json() == {"type": "searching"}


def test_find_match_timeout_sends_match_not_found(
    client: TestClient,
) -> None:
    client.app.state.play_queue = PlayQueue(timeout_seconds=0.05)
    credentials = register(client, "timeout_seeker")
    login(client, credentials)
    with connect(client) as socket:
        socket.receive_json()
        socket.send_json({"type": "find_match"})
        assert socket.receive_json() == {"type": "searching"}
        assert socket.receive_json() == {"type": "match_not_found"}


def test_disconnected_queued_player_is_not_later_paired(
    client: TestClient,
) -> None:
    first_credentials = register(client, "vanish_first")
    second_credentials = register(client, "vanish_second")

    login(client, first_credentials)
    with connect(client) as first_socket:
        first_socket.receive_json()
        first_socket.send_json({"type": "find_match"})
        assert first_socket.receive_json() == {"type": "searching"}
    # first_socket is disconnected here (context exited), which
    # must remove it from the queue.

    login(client, second_credentials)
    with connect(client) as second_socket:
        second_socket.receive_json()
        second_socket.send_json({"type": "find_match"})
        assert second_socket.receive_json() == {"type": "searching"}


def test_cancel_find_match_removes_waiting_player(
    client: TestClient,
) -> None:
    first_credentials = register(client, "cancel_first")
    second_credentials = register(client, "cancel_second")

    login(client, first_credentials)
    with connect(client) as first_socket:
        first_socket.receive_json()
        first_socket.send_json({"type": "find_match"})
        assert first_socket.receive_json() == {"type": "searching"}
        first_socket.send_json({"type": "cancel_find_match"})
        assert first_socket.receive_json() == {
            "type": "find_match_cancelled"
        }

        login(client, second_credentials)
        with connect(client) as second_socket:
            second_socket.receive_json()
            second_socket.send_json({"type": "find_match"})
            assert second_socket.receive_json() == {
                "type": "searching"
            }


def test_find_match_pairs_within_rating_range_inclusive(
    client: TestClient,
) -> None:
    first_credentials = register(client, "range_first")
    second_credentials = register(client, "range_second")
    # first stays at the default 1200; difference is exactly 100,
    # the inclusive boundary.
    set_rating(client, "range_second", 1300)

    login(client, first_credentials)
    with connect(client) as first_socket:
        first_socket.receive_json()
        first_socket.send_json({"type": "find_match"})
        assert first_socket.receive_json() == {"type": "searching"}

        login(client, second_credentials)
        with connect(client) as second_socket:
            second_socket.receive_json()
            second_socket.send_json({"type": "find_match"})

            first_ready = first_socket.receive_json()
            second_ready = second_socket.receive_json()
            assert first_ready["type"] == "match_ready"
            assert second_ready["type"] == "match_ready"
            assert (
                first_ready["room_id"] == second_ready["room_id"]
            )


def test_find_match_does_not_pair_outside_rating_range(
    client: TestClient,
) -> None:
    first_credentials = register(client, "outrange_first")
    second_credentials = register(client, "outrange_second")
    # Difference is 101 - just outside the inclusive +/-100 range.
    set_rating(client, "outrange_second", 1301)

    login(client, first_credentials)
    with connect(client) as first_socket:
        first_socket.receive_json()
        first_socket.send_json({"type": "find_match"})
        assert first_socket.receive_json() == {"type": "searching"}

        login(client, second_credentials)
        with connect(client) as second_socket:
            second_socket.receive_json()
            second_socket.send_json({"type": "find_match"})
            assert second_socket.receive_json() == {
                "type": "searching"
            }


def test_find_match_prefers_in_range_waiter_over_older_out_of_range(
    client: TestClient,
) -> None:
    # A (1200) waits first. B (1450) arrives next but is out of
    # range of A. C (1250) arrives last and is in range of A (and
    # closer to A than B is) - C must pair with A, not with B, even
    # though B has been waiting longer than C.
    a_credentials = register(client, "abc_a")
    b_credentials = register(client, "abc_b")
    c_credentials = register(client, "abc_c")
    set_rating(client, "abc_b", 1450)
    set_rating(client, "abc_c", 1250)

    login(client, a_credentials)
    with connect(client) as a_socket:
        a_socket.receive_json()
        a_socket.send_json({"type": "find_match"})
        assert a_socket.receive_json() == {"type": "searching"}

        login(client, b_credentials)
        with connect(client) as b_socket:
            b_socket.receive_json()
            b_socket.send_json({"type": "find_match"})
            assert b_socket.receive_json() == {"type": "searching"}

            login(client, c_credentials)
            with connect(client) as c_socket:
                c_socket.receive_json()
                c_socket.send_json({"type": "find_match"})

                a_ready = a_socket.receive_json()
                c_ready = c_socket.receive_json()
                assert a_ready["type"] == "match_ready"
                assert c_ready["type"] == "match_ready"
                assert a_ready["room_id"] == c_ready["room_id"]


def test_find_match_tie_break_prefers_longest_waiting(
    client: TestClient,
) -> None:
    # A (1100) and B (1300) are 200 apart - out of range of each
    # other, so B queues rather than pairing with A. C (1200) is
    # then exactly 100 from both A and B: an equal-distance tie.
    # The tie must go to A, who has been waiting longer than B.
    a_credentials = register(client, "tie_a")
    b_credentials = register(client, "tie_b")
    c_credentials = register(client, "tie_c")
    set_rating(client, "tie_a", 1100)
    set_rating(client, "tie_b", 1300)

    login(client, a_credentials)
    with connect(client) as a_socket:
        a_socket.receive_json()
        a_socket.send_json({"type": "find_match"})
        assert a_socket.receive_json() == {"type": "searching"}

        login(client, b_credentials)
        with connect(client) as b_socket:
            b_socket.receive_json()
            b_socket.send_json({"type": "find_match"})
            assert b_socket.receive_json() == {"type": "searching"}

            login(client, c_credentials)
            with connect(client) as c_socket:
                c_socket.receive_json()
                c_socket.send_json({"type": "find_match"})

                a_ready = a_socket.receive_json()
                c_ready = c_socket.receive_json()
                assert a_ready["type"] == "match_ready"
                assert c_ready["type"] == "match_ready"
                assert a_ready["room_id"] == c_ready["room_id"]
