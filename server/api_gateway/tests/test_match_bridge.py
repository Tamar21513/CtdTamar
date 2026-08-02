import asyncio
from functools import wraps
import os
from uuid import uuid4

import pytest
from sqlalchemy import select

from app.db.models import MatchHistory, User
from app.matches.manager import MatchManager, MatchOperationError
from app.matches.bridge import GameServerBridge
from app.matches.schemas import match_ready_message
from app.rooms.models import ConnectedUser, GameRoom


INITIAL_STATE = {
    "serverTimeMs": 1,
    "gameOver": False,
    "whiteScore": 0,
    "blackScore": 0,
    "whiteUsername": "host",
    "blackUsername": "guest",
    "playersReady": True,
    "gameplayStartsAtEpochMs": 1000,
    "pieces": [{"id": 1, "token": "wP"}],
    "motions": [],
    "jumps": [],
    "whiteMoveHistory": [],
    "blackMoveHistory": [],
}


def async_test(function):
    @wraps(function)
    def run():
        return asyncio.run(function())

    return run


class FakeSocket:
    def __init__(self) -> None:
        self.messages: list[dict] = []

    async def send_json(self, message: dict) -> None:
        self.messages.append(message)


class FakeBridge:
    instances: list["FakeBridge"] = []

    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.handler = None
        self.moves: list[tuple] = []
        self.jumps: list[tuple] = []
        self.closed = False
        self.__class__.instances.append(self)

    async def start(self, white, black, handler):
        self.handler = handler
        return INITIAL_STATE

    async def send_move(
        self, color, sequence, source, destination
    ) -> None:
        self.moves.append(
            (color, sequence, source, destination)
        )

    async def send_jump(self, color, sequence, cell) -> None:
        self.jumps.append((color, sequence, cell))

    async def close(self) -> None:
        self.closed = True


def active_room(
    name: str = "Arena",
    white_username: str = "host",
    black_username: str = "guest",
) -> GameRoom:
    host = ConnectedUser(uuid4(), white_username, FakeSocket())
    guest = ConnectedUser(uuid4(), black_username, FakeSocket())
    return GameRoom(
        room_id=uuid4(),
        name=name,
        visibility="public",
        host=host,
        guest=guest,
        white=host,
        black=guest,
        status="active",
    )


@async_test
async def test_players_share_snapshot_and_colors() -> None:
    FakeBridge.instances.clear()
    room = active_room()
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(room)

    white = room.white.websocket.messages[-1]
    black = room.black.websocket.messages[-1]
    assert white["type"] == black["type"] == "match_ready"
    assert white["state"] == black["state"] == INITIAL_STATE
    assert white["revision"] == black["revision"] == 1
    assert white["color"] == "white"
    assert black["color"] == "black"
    assert white["game_starts_at"] == black["game_starts_at"]


@async_test
async def test_countdown_blocks_actions_and_starts_once() -> None:
    FakeBridge.instances.clear()
    room = active_room()
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(room)
    task = manager._matches[room.room_id].countdown_task
    with pytest.raises(MatchOperationError) as move_error:
        await manager.move(
            room.room_id, room.white.user_id,
            room.white.websocket, 1,
            {"row": 6, "col": 4}, {"row": 5, "col": 4},
        )
    assert move_error.value.code == "match_not_started"
    with pytest.raises(MatchOperationError) as jump_error:
        await manager.jump(
            room.room_id, room.white.user_id,
            room.white.websocket, 2, {"row": 6, "col": 4},
        )
    assert jump_error.value.code == "match_not_started"
    await asyncio.sleep(3.9)
    messages = room.white.websocket.messages
    assert [m["value"] for m in messages
            if m["type"] == "match_countdown"] == [3, 2, 1, "GO"]
    assert messages[-1]["type"] == "match_started"
    assert manager._matches[room.room_id].revision == 1
    assert manager._matches[room.room_id].countdown_task is None
    assert task is not None
    await manager.cleanup_room(room.room_id)


@async_test
async def test_disconnect_cleanup_cancels_prestart_match() -> None:
    FakeBridge.instances.clear()
    room = active_room()
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(room)
    task = manager._matches[room.room_id].countdown_task
    await manager.cleanup_room(room.room_id)
    await asyncio.sleep(0)
    assert task is not None and task.cancelled()
    assert FakeBridge.instances[-1].closed
    assert room.white.websocket.messages[-1] == {
        "type": "match_cancelled",
        "room_id": str(room.room_id),
        "reason": "player_disconnected_before_start",
    }


@async_test
async def test_move_is_forwarded_and_cpp_result_is_preserved() -> None:
    FakeBridge.instances.clear()
    room = active_room()
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0
    bridge = FakeBridge.instances[-1]
    source = {"row": 6, "col": 4}
    destination = {"row": 5, "col": 4}

    await manager.move(
        room.room_id,
        room.white.user_id,
        room.white.websocket,
        7,
        source,
        destination,
    )
    assert bridge.moves == [
        ("white", 7, source, destination)
    ]

    await bridge.handler(
        "white",
        {
            "type": "move_rejected",
            "sequence": 7,
            "accepted": False,
            "reason": "cooldown_active",
            "hasSnapshot": True,
            "snapshot": {**INITIAL_STATE, "serverTimeMs": 2},
        },
    )
    assert room.white.websocket.messages[-2] == {
        "type": "move_result",
        "room_id": str(room.room_id),
        "sequence": 7,
        "accepted": False,
        "reason": "cooldown_active",
    }
    assert room.white.websocket.messages[-1]["type"] == (
        "match_state"
    )
    assert room.black.websocket.messages[-1]["revision"] == 2


@async_test
async def test_jump_is_forwarded_as_dedicated_action() -> None:
    FakeBridge.instances.clear()
    room = active_room()
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0
    bridge = FakeBridge.instances[-1]
    cell = {"row": 6, "col": 4}
    before = INITIAL_STATE["pieces"][0].copy()
    await manager.jump(
        room.room_id,
        room.white.user_id,
        room.white.websocket,
        9,
        cell,
    )
    assert bridge.jumps == [("white", 9, cell)]
    assert INITIAL_STATE["pieces"][0] == before


@async_test
async def test_spectator_snapshot_updates_and_read_only() -> None:
    FakeBridge.instances.clear()
    room = active_room()
    spectator = ConnectedUser(uuid4(), "viewer", FakeSocket())
    room.spectators[1] = spectator
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0
    await manager.watch_match(
        room.room_id, spectator.websocket
    )
    assert spectator.websocket.messages[-1]["type"] == (
        "match_snapshot"
    )

    with pytest.raises(MatchOperationError) as error:
        await manager.move(
            room.room_id,
            spectator.user_id,
            spectator.websocket,
            1,
            {"row": 1, "col": 1},
            {"row": 2, "col": 1},
        )
    assert error.value.code == "not_a_player"

    bridge = FakeBridge.instances[-1]
    await bridge.handler(
        "black",
        {
            "type": "game_state_updated",
            "hasSnapshot": True,
            "snapshot": {**INITIAL_STATE, "serverTimeMs": 3},
        },
    )
    assert spectator.websocket.messages[-1]["type"] == (
        "match_state"
    )


@async_test
async def test_revision_only_advances_for_changed_snapshot() -> None:
    FakeBridge.instances.clear()
    room = active_room()
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(room)
    bridge = FakeBridge.instances[-1]
    await bridge.handler(
        "white",
        {
            "type": "game_state_updated",
            "hasSnapshot": True,
            "snapshot": INITIAL_STATE,
        },
    )
    assert len(room.white.websocket.messages) == 1


@async_test
async def test_cleanup_releases_single_match_allocator() -> None:
    FakeBridge.instances.clear()
    first = active_room("First")
    second = active_room("Second")
    manager = MatchManager("game", 54000, FakeBridge)
    await manager.start_match(first)
    with pytest.raises(MatchOperationError) as error:
        await manager.start_match(second)
    assert error.value.code == "match_unavailable"

    await manager.cleanup_room(first.room_id)
    assert FakeBridge.instances[0].closed
    await manager.start_match(second)


@async_test
async def test_shard_pool_allows_concurrent_matches_on_different_shards() -> None:
    FakeBridge.instances.clear()
    first = active_room("First")
    second = active_room("Second")
    third = active_room("Third")
    manager = MatchManager(
        "shard-a",
        54000,
        FakeBridge,
        additional_shards=[("shard-b", 54001)],
    )

    await asyncio.gather(
        manager.start_match(first), manager.start_match(second)
    )

    used_shards = {
        (instance.host, instance.port)
        for instance in FakeBridge.instances
    }
    assert used_shards == {("shard-a", 54000), ("shard-b", 54001)}

    with pytest.raises(MatchOperationError) as error:
        await manager.start_match(third)
    assert error.value.code == "match_unavailable"

    await manager.cleanup_room(first.room_id)
    await manager.start_match(third)
    used_shards = {
        (instance.host, instance.port)
        for instance in FakeBridge.instances
    }
    assert used_shards == {("shard-a", 54000), ("shard-b", 54001)}


@async_test
async def test_game_over_releases_match_and_notifies_lifecycle() -> None:
    FakeBridge.instances.clear()
    ended: list = []

    async def match_ended(room_id) -> None:
        ended.append(room_id)

    first = active_room("First")
    second = active_room("Second")
    manager = MatchManager(
        "game", 54000, FakeBridge, match_ended
    )
    await manager.start_match(first)
    bridge = FakeBridge.instances[-1]
    await bridge.handler(
        "white",
        {
            "type": "game_over",
            "hasSnapshot": True,
            "snapshot": {**INITIAL_STATE, "gameOver": True},
        },
    )
    await asyncio.sleep(0)
    assert ended == [first.room_id]
    assert bridge.closed
    await manager.start_match(second)


def test_match_ready_message_includes_ratings() -> None:
    message = match_ready_message(
        uuid4(),
        "white",
        "opponent",
        1,
        {"a": 1},
        "2024-01-01T00:00:00Z",
        1350,
        1180,
    )
    assert message["white_rating"] == 1350
    assert message["black_rating"] == 1180


def test_win_by_king_capture_pushes_rating_updated_to_both_players(
    client,
) -> None:
    asyncio.run(_win_pushes_rating_updated_to_both_players(client))


async def _win_pushes_rating_updated_to_both_players(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="PushWinner",
                    username_normalized="pushwinner",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="PushLoser",
                    username_normalized="pushloser",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="PushWinner",
        black_username="PushLoser",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
    )
    await manager.start_match(room)
    bridge = FakeBridge.instances[-1]

    await bridge.handler(
        "white",
        {
            "type": "game_over",
            "hasSnapshot": True,
            "snapshot": {
                **INITIAL_STATE,
                "gameOver": True,
                "pieces": [{"id": 1, "token": "wK"}],
            },
        },
    )
    await asyncio.sleep(0.2)

    winner_updates = [
        message
        for message in room.white.websocket.messages
        if message["type"] == "rating_updated"
    ]
    loser_updates = [
        message
        for message in room.black.websocket.messages
        if message["type"] == "rating_updated"
    ]
    assert winner_updates == [
        {"type": "rating_updated", "rating": 1216}
    ]
    assert loser_updates == [
        {"type": "rating_updated", "rating": 1184}
    ]


def test_win_by_king_capture_updates_both_players_ratings(
    client,
) -> None:
    asyncio.run(_win_updates_both_players_ratings(client))


async def _win_updates_both_players_ratings(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="RatingWinner",
                    username_normalized="ratingwinner",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="RatingLoser",
                    username_normalized="ratingloser",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="RatingWinner",
        black_username="RatingLoser",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
    )
    await manager.start_match(room)
    bridge = FakeBridge.instances[-1]

    await bridge.handler(
        "white",
        {
            "type": "game_over",
            "hasSnapshot": True,
            "snapshot": {
                **INITIAL_STATE,
                "gameOver": True,
                # Only the white king survives in the final
                # snapshot - captured pieces (including a
                # captured king) are omitted from `pieces`
                # entirely, so this is how a win is recognized.
                "pieces": [{"id": 1, "token": "wK"}],
            },
        },
    )
    await asyncio.sleep(0.2)

    with client.app.state.db_session_factory() as session:
        winner = session.scalar(
            select(User).where(User.username == "RatingWinner")
        )
        loser = session.scalar(
            select(User).where(User.username == "RatingLoser")
        )
        assert winner.rating == 1216
        assert loser.rating == 1184


def test_win_by_king_capture_records_match_history(client) -> None:
    asyncio.run(_win_records_match_history(client))


async def _win_records_match_history(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="HistoryWinner",
                    username_normalized="historywinner",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="HistoryLoser",
                    username_normalized="historyloser",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="HistoryLoser",
        black_username="HistoryWinner",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
    )
    await manager.start_match(room)
    bridge = FakeBridge.instances[-1]

    await bridge.handler(
        "black",
        {
            "type": "game_over",
            "hasSnapshot": True,
            # Only the black king survives - black (HistoryWinner)
            # wins.
            "snapshot": {
                **INITIAL_STATE,
                "gameOver": True,
                "pieces": [{"id": 1, "token": "bK"}],
            },
        },
    )
    await asyncio.sleep(0.2)

    with client.app.state.db_session_factory() as session:
        rows = session.scalars(select(MatchHistory)).all()
        assert len(rows) == 1
        row = rows[0]
        assert row.room_id == room.room_id
        assert row.white_username == "HistoryLoser"
        assert row.black_username == "HistoryWinner"
        assert row.winner_color == "black"
        assert row.white_rating_before == 1200
        assert row.white_rating_after == 1184
        assert row.black_rating_before == 1200
        assert row.black_rating_after == 1216
        assert row.reason == "king_capture"


def test_mid_match_disconnect_updates_ratings(client) -> None:
    asyncio.run(_mid_match_disconnect_updates_ratings(client))


async def _mid_match_disconnect_updates_ratings(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="StayingPlayer",
                    username_normalized="stayingplayer",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="LeavingPlayer",
                    username_normalized="leavingplayer",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="StayingPlayer",
        black_username="LeavingPlayer",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
        disconnect_grace_seconds=0.05,
    )
    await manager.start_match(room)
    # Move the match past its countdown so cleanup treats this as
    # a mid-match disconnect, not a pre-start cancellation.
    manager._matches[room.room_id].game_starts_at = 0

    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )
    # The disconnect only starts a grace period now - ratings must
    # not change until it actually expires.
    with client.app.state.db_session_factory() as session:
        staying = session.scalar(
            select(User).where(User.username == "StayingPlayer")
        )
        leaving = session.scalar(
            select(User).where(User.username == "LeavingPlayer")
        )
        assert staying.rating == 1200
        assert leaving.rating == 1200

    await asyncio.sleep(0.1)

    with client.app.state.db_session_factory() as session:
        staying = session.scalar(
            select(User).where(User.username == "StayingPlayer")
        )
        leaving = session.scalar(
            select(User).where(User.username == "LeavingPlayer")
        )
        assert staying.rating == 1216
        assert leaving.rating == 1184


def test_mid_match_disconnect_records_match_history(client) -> None:
    asyncio.run(_mid_match_disconnect_records_match_history(client))


async def _mid_match_disconnect_records_match_history(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="DiscHistoryStaying",
                    username_normalized="dischistorystaying",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="DiscHistoryLeaving",
                    username_normalized="dischistoryleaving",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="DiscHistoryStaying",
        black_username="DiscHistoryLeaving",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
        disconnect_grace_seconds=0.05,
    )
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0

    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )
    # No history row until the grace period actually expires.
    with client.app.state.db_session_factory() as session:
        assert session.scalars(select(MatchHistory)).all() == []

    await asyncio.sleep(0.1)

    with client.app.state.db_session_factory() as session:
        rows = session.scalars(select(MatchHistory)).all()
        assert len(rows) == 1
        row = rows[0]
        assert row.room_id == room.room_id
        assert row.winner_color == "white"
        assert row.white_rating_before == 1200
        assert row.white_rating_after == 1216
        assert row.black_rating_before == 1200
        assert row.black_rating_after == 1184
        assert row.reason == "disconnect"


def test_disconnect_before_match_start_does_not_change_ratings(
    client,
) -> None:
    asyncio.run(
        _disconnect_before_match_start_does_not_change_ratings(
            client
        )
    )


async def _disconnect_before_match_start_does_not_change_ratings(
    client,
) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="PrestartStaying",
                    username_normalized="prestartstaying",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="PrestartLeaving",
                    username_normalized="prestartleaving",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="PrestartStaying",
        black_username="PrestartLeaving",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
    )
    await manager.start_match(room)
    # game_starts_at is left in the future (the default 3.8s
    # countdown set by start_match), so this is a pre-start
    # disconnect - the match_cancelled path, not a loss.

    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )

    with client.app.state.db_session_factory() as session:
        staying = session.scalar(
            select(User).where(User.username == "PrestartStaying")
        )
        leaving = session.scalar(
            select(User).where(User.username == "PrestartLeaving")
        )
        assert staying.rating == 1200
        assert leaving.rating == 1200


def test_prestart_disconnect_creates_no_history_row(client) -> None:
    asyncio.run(_prestart_disconnect_creates_no_history_row(client))


async def _prestart_disconnect_creates_no_history_row(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="NoHistoryStaying",
                    username_normalized="nohistorystaying",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="NoHistoryLeaving",
                    username_normalized="nohistoryleaving",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="NoHistoryStaying",
        black_username="NoHistoryLeaving",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
    )
    await manager.start_match(room)
    # game_starts_at is left in the future, so this is a pre-start
    # disconnect (match_cancelled path) - no rating change and no
    # history row either.

    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )

    with client.app.state.db_session_factory() as session:
        rows = session.scalars(select(MatchHistory)).all()
        assert rows == []


def test_disconnect_after_normal_game_over_does_not_double_update(
    client,
) -> None:
    asyncio.run(
        _disconnect_after_normal_game_over_does_not_double_update(
            client
        )
    )


async def _disconnect_after_normal_game_over_does_not_double_update(
    client,
) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="FinishedWinner",
                    username_normalized="finishedwinner",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="FinishedLoser",
                    username_normalized="finishedloser",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="FinishedWinner",
        black_username="FinishedLoser",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
    )
    await manager.start_match(room)
    bridge = FakeBridge.instances[-1]

    await bridge.handler(
        "white",
        {
            "type": "game_over",
            "hasSnapshot": True,
            "snapshot": {
                **INITIAL_STATE,
                "gameOver": True,
                "pieces": [{"id": 1, "token": "wK"}],
            },
        },
    )
    # Let _finish_match's task run to completion, popping the
    # match and applying the king-capture rating update.
    await asyncio.sleep(0.2)

    with client.app.state.db_session_factory() as session:
        winner = session.scalar(
            select(User).where(User.username == "FinishedWinner")
        )
        loser = session.scalar(
            select(User).where(User.username == "FinishedLoser")
        )
        assert winner.rating == 1216
        assert loser.rating == 1184

    # A late disconnect notification for the same, already-finished
    # room must not apply a second rating update. The match was
    # already popped by _finish_match, so cleanup_room finds
    # nothing left to process.
    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )

    with client.app.state.db_session_factory() as session:
        winner = session.scalar(
            select(User).where(User.username == "FinishedWinner")
        )
        loser = session.scalar(
            select(User).where(User.username == "FinishedLoser")
        )
        assert winner.rating == 1216
        assert loser.rating == 1184


def test_mid_match_disconnect_starts_grace_period_without_immediate_rating_change(
    client,
) -> None:
    asyncio.run(_disconnect_starts_grace_period(client))


async def _disconnect_starts_grace_period(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="GraceStaying",
                    username_normalized="gracestaying",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="GraceLeaving",
                    username_normalized="graceleaving",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="GraceStaying",
        black_username="GraceLeaving",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
        disconnect_grace_seconds=12,
    )
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0

    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )

    # The grace period has only just started - the match is still
    # tracked and nothing has been finalized yet.
    assert room.room_id in manager._matches
    with client.app.state.db_session_factory() as session:
        staying = session.scalar(
            select(User).where(User.username == "GraceStaying")
        )
        leaving = session.scalar(
            select(User).where(User.username == "GraceLeaving")
        )
        assert staying.rating == 1200
        assert leaving.rating == 1200

    assert room.white.websocket.messages[-1] == {
        "type": "opponent_reconnecting",
        "room_id": str(room.room_id),
        "seconds_remaining": 12,
    }

    manager._matches[room.room_id].disconnect_grace_task.cancel()


@async_test
async def test_reclaim_resumes_match_and_resets_disconnect_state() -> None:
    FakeBridge.instances.clear()
    room = active_room(
        white_username="ReclaimStaying",
        black_username="ReclaimLeaving",
    )
    manager = MatchManager(
        "game", 54000, FakeBridge, disconnect_grace_seconds=12
    )
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0
    leaving_user_id = room.black.user_id

    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=leaving_user_id,
    )
    assert (
        manager._matches[room.room_id].disconnected_user_id
        == leaving_user_id
    )

    new_socket = FakeSocket()
    reclaimed = await manager.try_reclaim(leaving_user_id, new_socket)
    assert reclaimed is True

    match = manager._matches[room.room_id]
    assert match.disconnected_user_id is None
    assert match.disconnect_grace_task is None
    assert match.room.black.websocket is new_socket

    resumed_message = new_socket.messages[-1]
    assert resumed_message["type"] == "match_resumed"
    assert resumed_message["room_id"] == str(room.room_id)
    assert resumed_message["color"] == "black"
    assert resumed_message["opponent"] == "ReclaimStaying"
    assert resumed_message["revision"] == match.revision
    assert resumed_message["state"] == match.snapshot
    assert resumed_message["white_rating"] == 1200
    assert resumed_message["black_rating"] == 1200

    assert room.white.websocket.messages[-1] == {
        "type": "opponent_reconnected",
        "room_id": str(room.room_id),
    }

    # Reclaiming must truly reset disconnected_user_id - a fresh
    # disconnect of the same user has to start a brand-new grace
    # period, not silently no-op because it looks "already handled".
    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=leaving_user_id,
    )
    match = manager._matches[room.room_id]
    assert match.disconnected_user_id == leaving_user_id
    assert match.disconnect_grace_task is not None
    match.disconnect_grace_task.cancel()


def test_disconnect_grace_period_expiry_matches_immediate_disconnect_outcome(
    client,
) -> None:
    asyncio.run(_grace_period_expiry_outcome(client))


async def _grace_period_expiry_outcome(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="ExpiryStaying",
                    username_normalized="expirystaying",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="ExpiryLeaving",
                    username_normalized="expiryleaving",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="ExpiryStaying",
        black_username="ExpiryLeaving",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
        disconnect_grace_seconds=0.05,
    )
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0

    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )
    await asyncio.sleep(0.15)

    # Match is fully finalized once the grace period expires.
    assert room.room_id not in manager._matches
    assert FakeBridge.instances[-1].closed
    with client.app.state.db_session_factory() as session:
        staying = session.scalar(
            select(User).where(User.username == "ExpiryStaying")
        )
        leaving = session.scalar(
            select(User).where(User.username == "ExpiryLeaving")
        )
        assert staying.rating == 1216
        assert leaving.rating == 1184
        rows = session.scalars(select(MatchHistory)).all()
        assert len(rows) == 1
        assert rows[0].reason == "disconnect"
        assert rows[0].winner_color == "white"
    assert any(
        message.get("type") == "match_forfeited"
        for message in room.white.websocket.messages
    )


def test_second_disconnect_during_grace_finalizes_with_first_as_loser(
    client,
) -> None:
    asyncio.run(_second_disconnect_during_grace(client))


async def _second_disconnect_during_grace(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="FirstToLeave",
                    username_normalized="firsttoleave",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="SecondToLeave",
                    username_normalized="secondtoleave",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="FirstToLeave",
        black_username="SecondToLeave",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
        # Long enough that the second disconnect below always lands
        # while the first player's grace period is still active.
        disconnect_grace_seconds=30,
    )
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0

    # White disconnects first and enters its grace period.
    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.white.user_id,
    )
    assert (
        manager._matches[room.room_id].disconnected_user_id
        == room.white.user_id
    )

    # Black then also disconnects while white is still in grace -
    # this must finalize immediately, with white (the FIRST
    # disconnected user) as the loser, not black.
    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )

    assert room.room_id not in manager._matches
    with client.app.state.db_session_factory() as session:
        first = session.scalar(
            select(User).where(User.username == "FirstToLeave")
        )
        second = session.scalar(
            select(User).where(User.username == "SecondToLeave")
        )
        assert first.rating == 1184
        assert second.rating == 1216
        rows = session.scalars(select(MatchHistory)).all()
        assert len(rows) == 1
        assert rows[0].winner_color == "black"


def test_king_capture_during_grace_period_cancels_grace_and_wins_by_capture(
    client,
) -> None:
    asyncio.run(_king_capture_during_grace_period(client))


async def _king_capture_during_grace_period(client) -> None:
    FakeBridge.instances.clear()
    with client.app.state.db_session_factory() as session:
        session.add_all(
            [
                User(
                    username="CaptureStaying",
                    username_normalized="capturestaying",
                    password_hash="not-a-real-hash",
                ),
                User(
                    username="CaptureLeaving",
                    username_normalized="captureleaving",
                    password_hash="not-a-real-hash",
                ),
            ]
        )
        session.commit()

    room = active_room(
        white_username="CaptureStaying",
        black_username="CaptureLeaving",
    )
    manager = MatchManager(
        "game",
        54000,
        FakeBridge,
        session_factory=client.app.state.db_session_factory,
        disconnect_grace_seconds=30,
    )
    await manager.start_match(room)
    manager._matches[room.room_id].game_starts_at = 0
    bridge = FakeBridge.instances[-1]

    # Black disconnects and enters a long grace period - the bridge
    # to chess_server is independent of the Gateway websocket, so
    # the still-connected white player can keep playing and win by
    # king capture while black's grace period is running.
    await manager.cleanup_room(
        room.room_id,
        disconnected_user_id=room.black.user_id,
    )
    match = manager._matches[room.room_id]
    grace_task = match.disconnect_grace_task
    assert grace_task is not None
    assert not grace_task.done()

    await bridge.handler(
        "white",
        {
            "type": "game_over",
            "hasSnapshot": True,
            "snapshot": {
                **INITIAL_STATE,
                "gameOver": True,
                "pieces": [{"id": 1, "token": "wK"}],
            },
        },
    )
    await asyncio.sleep(0.2)

    # No exception/hang, and the grace task was cancelled rather
    # than left to expire on its own later.
    assert grace_task.cancelled()
    assert room.room_id not in manager._matches
    with client.app.state.db_session_factory() as session:
        staying = session.scalar(
            select(User).where(User.username == "CaptureStaying")
        )
        leaving = session.scalar(
            select(User).where(User.username == "CaptureLeaving")
        )
        # King-capture logic determined the outcome, not the
        # pending disconnect - white (the king-capture winner) gains
        # rating even though black was the one mid-grace-period.
        assert staying.rating == 1216
        assert leaving.rating == 1184
        rows = session.scalars(select(MatchHistory)).all()
        assert len(rows) == 1
        assert rows[0].reason == "king_capture"
        assert rows[0].winner_color == "white"


@async_test
async def test_live_cpp_bridge_sequence_and_authority() -> None:
    if os.getenv("CTD_RUN_LIVE_GAME_BRIDGE") is None:
        return
    host = os.getenv("CTD_GAME_SERVER_HOST", "127.0.0.1")
    port = int(os.getenv("CTD_GAME_SERVER_PORT", "5050"))
    events: asyncio.Queue[tuple[str, dict]] = asyncio.Queue()

    async def receive(color: str, message: dict) -> None:
        await events.put((color, message))

    bridge = GameServerBridge(host, port)
    snapshot = await bridge.start(
        "bridge_white", "bridge_black", receive
    )
    try:
        assert snapshot["playersReady"] is True
        assert snapshot["pieces"]
        await asyncio.sleep(4.1)

        async def result_for(sequence: int) -> tuple[str, dict]:
            while True:
                color, result = await asyncio.wait_for(
                    events.get(), timeout=5.0
                )
                if result.get("sequence") == sequence:
                    return color, result

        await bridge.send_move(
            "white",
            41,
            {"row": 6, "col": 4},
            {"row": 5, "col": 4},
        )
        color, result = await result_for(41)
        assert color == "white"
        assert result["type"] == "move_accepted"
        assert result["accepted"] is True
        assert result["hasSnapshot"] is True

        await bridge.send_move(
            "black",
            42,
            {"row": 6, "col": 3},
            {"row": 5, "col": 3},
        )
        color, result = await result_for(42)
        assert color == "black"
        assert result["type"] == "move_rejected"
        assert result["reason"] == "not_your_piece"

        await bridge.send_move(
            "white",
            43,
            {"row": 6, "col": 0},
            {"row": 5, "col": 1},
        )
        color, result = await result_for(43)
        assert color == "white"
        assert result["type"] == "move_rejected"
        assert result["accepted"] is False

        await bridge.send_jump(
            "white",
            44,
            {"row": 6, "col": 0},
        )
        color, result = await result_for(44)
        assert color == "white"
        assert result["type"] == "move_accepted"
        assert result["reason"] == "jump_started"
        jumping = result["snapshot"]
        assert jumping["jumps"][0]["cell"] == {
            "row": 6, "col": 0
        }
        assert next(
            piece
            for piece in jumping["pieces"]
            if piece["id"] == jumping["jumps"][0]["pieceId"]
        )["position"] == {"row": 6, "col": 0}

        saw_short_rest = False
        saw_alive_again = False
        deadline = asyncio.get_running_loop().time() + 6
        while asyncio.get_running_loop().time() < deadline:
            _, update = await asyncio.wait_for(
                events.get(), timeout=2.0
            )
            state = update.get("snapshot")
            if not isinstance(state, dict):
                continue
            pawn = next(
                (
                    piece
                    for piece in state.get("pieces", [])
                    if piece["position"] == {"row": 6, "col": 0}
                ),
                None,
            )
            if pawn is None:
                continue
            if (
                not state.get("jumps")
                and pawn["remainingCooldownMs"] > 0
            ):
                saw_short_rest = True
            if (
                saw_short_rest
                and pawn["remainingCooldownMs"] == 0
            ):
                saw_alive_again = True
                break
        assert saw_short_rest
        assert saw_alive_again
    finally:
        await bridge.close()
