import asyncio
import logging
import time
from datetime import datetime, timezone
from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field
from typing import Any, Protocol
from uuid import UUID

from fastapi import WebSocket
from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.orm import Session, sessionmaker

from app.db.models import User
from app.matches.allocator import (
    MatchUnavailableError,
    SingleMatchAllocator,
)
from app.matches.bridge import GameServerBridge
from app.matches.rating import update_ratings
from app.matches.schemas import (
    match_ready_message,
    match_countdown_message,
    match_started_message,
    match_cancelled_message,
    match_snapshot_message,
    match_state_message,
    move_result_message,
)
from app.rooms.models import GameRoom


logger = logging.getLogger(__name__)


class MatchOperationError(RuntimeError):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.message = message


class MatchBridge(Protocol):
    async def start(self, white_username, black_username, handler): ...
    async def send_move(self, color, sequence, source, destination): ...
    async def send_jump(self, color, sequence, cell): ...
    async def close(self): ...


@dataclass
class ActiveMatch:
    room: GameRoom
    bridge: MatchBridge
    revision: int
    snapshot: dict[str, Any]
    game_starts_at: float
    game_starts_at_iso: str
    countdown_task: asyncio.Task | None = None
    sequence_owners: dict[tuple[str, int], WebSocket] = field(
        default_factory=dict
    )


class MatchManager:
    def __init__(
        self,
        host: str,
        port: int,
        bridge_factory=GameServerBridge,
        match_ended_handler: (
            Callable[[UUID], Awaitable[None]] | None
        ) = None,
        session_factory: sessionmaker[Session] | None = None,
    ) -> None:
        self._host = host
        self._port = port
        self._bridge_factory = bridge_factory
        self._match_ended_handler = match_ended_handler
        self._session_factory = session_factory
        self._allocator = SingleMatchAllocator()
        self._matches: dict[UUID, ActiveMatch] = {}
        self._lock = asyncio.Lock()

    @staticmethod
    async def _send(websocket: WebSocket, message: dict) -> None:
        try:
            await websocket.send_json(message)
        except RuntimeError:
            pass

    async def start_match(self, room: GameRoom) -> None:
        if room.white is None or room.black is None:
            logger.warning(
                "match_reject room_id=%s code=room_not_ready",
                room.room_id,
            )
            raise MatchOperationError(
                "room_not_ready", "The room needs two players."
            )
        try:
            await self._allocator.acquire()
        except MatchUnavailableError as error:
            logger.warning(
                "match_reject room_id=%s code=match_unavailable",
                room.room_id,
            )
            raise MatchOperationError(
                "match_unavailable",
                "The game server is already hosting a match.",
            ) from error

        bridge = self._bridge_factory(self._host, self._port)

        async def handle(color: str, message: dict[str, Any]) -> None:
            await self._handle_bridge_event(room.room_id, color, message)

        try:
            snapshot = await bridge.start(
                room.white.username,
                room.black.username,
                handle,
            )
            game_starts_at = time.time() + 3.8
            game_starts_at_iso = datetime.fromtimestamp(
                game_starts_at, timezone.utc
            ).isoformat().replace("+00:00", "Z")
            match = ActiveMatch(
                room, bridge, 1, snapshot, game_starts_at,
                game_starts_at_iso,
            )
            async with self._lock:
                self._matches[room.room_id] = match
            await self._send(
                room.white.websocket,
                match_ready_message(
                    room.room_id,
                    "white",
                    room.black.username,
                    1,
                    snapshot,
                    game_starts_at_iso,
                ),
            )
            await self._send(
                room.black.websocket,
                match_ready_message(
                    room.room_id,
                    "black",
                    room.white.username,
                    1,
                    snapshot,
                    game_starts_at_iso,
                ),
            )
            match.countdown_task = asyncio.create_task(
                self._run_countdown(room.room_id)
            )
            logger.info(
                "match_start room_id=%s white=%s black=%s",
                room.room_id,
                room.white.username,
                room.black.username,
            )
        except Exception:
            logger.error(
                "match_reject room_id=%s code=game_server_unavailable",
                room.room_id,
            )
            await bridge.close()
            await self._allocator.release()
            raise MatchOperationError(
                "game_server_unavailable",
                "The authoritative game server is unavailable.",
            )

    async def watch_match(
        self,
        room_id: UUID,
        websocket: WebSocket,
    ) -> None:
        async with self._lock:
            match = self._matches.get(room_id)
            if match is None:
                raise MatchOperationError(
                    "match_not_found",
                    "The authoritative match is not available.",
                )
            if time.time() < match.game_starts_at:
                message = match_ready_message(
                    room_id, "spectator", "", match.revision,
                    match.snapshot, match.game_starts_at_iso
                )
            else:
                message = match_snapshot_message(
                    room_id, match.revision, match.snapshot
                )
            countdown = (
                match_countdown_message(
                    room_id,
                    self._countdown_value(match.game_starts_at),
                    match.game_starts_at_iso,
                )
                if time.time() < match.game_starts_at
                else None
            )
        await self._send(websocket, message)
        if countdown is not None:
            await self._send(websocket, countdown)

    @staticmethod
    def _countdown_value(game_starts_at: float) -> int | str:
        remaining = game_starts_at - time.time()
        if remaining <= 0.8:
            return "GO"
        if remaining <= 1.8:
            return 1
        if remaining <= 2.8:
            return 2
        return 3

    async def _run_countdown(self, room_id: UUID) -> None:
        for value, elapsed in ((3, 0.0), (2, 1.0), (1, 2.0),
                               ("GO", 3.0)):
            async with self._lock:
                match = self._matches.get(room_id)
                if match is None:
                    return
                delay = (
                    match.game_starts_at - 3.8 + elapsed - time.time()
                )
                participants = [
                    match.room.white, match.room.black,
                    *match.room.spectators.values(),
                ]
                message = match_countdown_message(
                    room_id, value, match.game_starts_at_iso
                )
            if delay > 0:
                await asyncio.sleep(delay)
            await asyncio.gather(*(
                self._send(p.websocket, message)
                for p in participants if p is not None
            ))
        async with self._lock:
            match = self._matches.get(room_id)
            if match is None:
                return
            delay = match.game_starts_at - time.time()
        if delay > 0:
            await asyncio.sleep(delay)
        async with self._lock:
            match = self._matches.get(room_id)
            if match is None:
                return
            message = match_started_message(
                room_id, match.game_starts_at_iso,
                match.revision, match.snapshot
            )
            participants = [
                match.room.white, match.room.black,
                *match.room.spectators.values(),
            ]
        await asyncio.gather(*(
            self._send(p.websocket, message)
            for p in participants if p is not None
        ))
        async with self._lock:
            match = self._matches.get(room_id)
            if match is not None:
                match.countdown_task = None

    async def move(
        self,
        room_id: UUID,
        user_id: UUID,
        websocket: WebSocket,
        sequence: object,
        source: object,
        destination: object,
    ) -> None:
        if (
            not isinstance(sequence, int)
            or isinstance(sequence, bool)
            or sequence < 1
        ):
            raise MatchOperationError(
                "invalid_move", "Sequence must be a positive integer."
            )
        for value in (source, destination):
            if (
                not isinstance(value, dict)
                or not isinstance(value.get("row"), int)
                or not isinstance(value.get("col"), int)
                or not 0 <= value["row"] <= 7
                or not 0 <= value["col"] <= 7
            ):
                raise MatchOperationError(
                    "invalid_move",
                    "Move positions must be valid board cells.",
                )
        async with self._lock:
            match = self._matches.get(room_id)
            if match is None:
                raise MatchOperationError(
                    "match_not_found", "The match is not active."
                )
            if time.time() < match.game_starts_at:
                raise MatchOperationError(
                    "match_not_started",
                    "The match countdown is still active.",
                )
            room = match.room
            if room.white and room.white.user_id == user_id:
                color = "white"
            elif room.black and room.black.user_id == user_id:
                color = "black"
            else:
                raise MatchOperationError(
                    "not_a_player",
                    "Only room players may submit moves.",
                )
            key = (color, sequence)
            if key in match.sequence_owners:
                raise MatchOperationError(
                    "duplicate_sequence",
                    "Move sequence was already submitted.",
                )
            match.sequence_owners[key] = websocket
            bridge = match.bridge
        await bridge.send_move(
            color, sequence, source, destination
        )

    async def jump(
        self,
        room_id: UUID,
        user_id: UUID,
        websocket: WebSocket,
        sequence: object,
        cell: object,
    ) -> None:
        await self._validate_and_send_jump(
            room_id, user_id, websocket, sequence, cell
        )

    async def _validate_and_send_jump(
        self,
        room_id: UUID,
        user_id: UUID,
        websocket: WebSocket,
        sequence: object,
        cell: object,
    ) -> None:
        if (
            not isinstance(sequence, int)
            or isinstance(sequence, bool)
            or sequence < 1
            or not isinstance(cell, dict)
            or not isinstance(cell.get("row"), int)
            or not isinstance(cell.get("col"), int)
            or not 0 <= cell["row"] <= 7
            or not 0 <= cell["col"] <= 7
        ):
            raise MatchOperationError(
                "invalid_jump",
                "Jump requires a sequence and a valid board cell.",
            )
        async with self._lock:
            match = self._matches.get(room_id)
            if match is None:
                raise MatchOperationError(
                    "match_not_found", "The match is not active."
                )
            if time.time() < match.game_starts_at:
                raise MatchOperationError(
                    "match_not_started",
                    "The match countdown is still active.",
                )
            room = match.room
            if room.white and room.white.user_id == user_id:
                color = "white"
            elif room.black and room.black.user_id == user_id:
                color = "black"
            else:
                raise MatchOperationError(
                    "not_a_player",
                    "Only room players may submit jumps.",
                )
            key = (color, sequence)
            if key in match.sequence_owners:
                raise MatchOperationError(
                    "duplicate_sequence",
                    "Action sequence was already submitted.",
                )
            match.sequence_owners[key] = websocket
            bridge = match.bridge
        await bridge.send_jump(color, sequence, cell)

    async def _handle_bridge_event(
        self,
        room_id: UUID,
        color: str,
        message: dict[str, Any],
    ) -> None:
        message_type = message.get("type")
        deliveries: list[tuple[WebSocket, dict]] = []
        async with self._lock:
            match = self._matches.get(room_id)
            if match is None:
                return
            if message_type in {"move_accepted", "move_rejected"}:
                sequence = int(message.get("sequence", 0))
                owner = match.sequence_owners.pop(
                    (color, sequence), None
                )
                if owner is not None:
                    deliveries.append(
                        (
                            owner,
                            move_result_message(
                                room_id,
                                sequence,
                                bool(message.get("accepted")),
                                str(message.get("reason", "")),
                            ),
                        )
                    )
            if message.get("hasSnapshot") and isinstance(
                message.get("snapshot"), dict
            ):
                snapshot = message["snapshot"]
                if snapshot != match.snapshot:
                    match.snapshot = snapshot
                    match.revision += 1
                    update = match_state_message(
                        room_id, match.revision, snapshot
                    )
                    participants = [
                        match.room.white,
                        match.room.black,
                        *match.room.spectators.values(),
                    ]
                    deliveries.extend(
                        (participant.websocket, update)
                        for participant in participants
                        if participant is not None
                    )
        for websocket, outgoing in deliveries:
            await self._send(websocket, outgoing)
        if (
            message.get("hasSnapshot")
            and isinstance(message.get("snapshot"), dict)
            and message["snapshot"].get("gameOver") is True
        ):
            asyncio.create_task(self._finish_match(room_id))

    async def _finish_match(self, room_id: UUID) -> None:
        async with self._lock:
            match = self._matches.pop(room_id, None)
        if match is None:
            return
        logger.info("match_finish room_id=%s", room_id)
        if match.snapshot.get("gameOver") is True:
            await self._apply_rating_update(match)
        if match.countdown_task is not None:
            match.countdown_task.cancel()
        if self._match_ended_handler is not None:
            await self._match_ended_handler(room_id)
        await match.bridge.close()
        await self._allocator.release()

    @staticmethod
    def _winner_color(snapshot: dict[str, Any]) -> str | None:
        """Determines the winning color from the final snapshot.

        The wire snapshot carries no explicit winner field (only
        gameOver/whiteScore/blackScore cross the bridge - see
        shared/cpp Protocol.cpp). Captured pieces are omitted from
        the snapshot's `pieces` list entirely (see
        GameStateSnapshotBuilder.cpp), and king capture is the sole
        game-over trigger (see GameEngine.cpp's endGame call sites),
        so the color whose king ("wK"/"bK") is still present in
        `pieces` is reliably the winner.
        """
        pieces = snapshot.get("pieces")
        if not isinstance(pieces, list):
            return None
        tokens = {
            piece.get("token")
            for piece in pieces
            if isinstance(piece, dict)
        }
        white_king_alive = "wK" in tokens
        black_king_alive = "bK" in tokens
        if white_king_alive and not black_king_alive:
            return "white"
        if black_king_alive and not white_king_alive:
            return "black"
        return None

    async def _apply_rating_update(self, match: ActiveMatch) -> None:
        if self._session_factory is None:
            return
        winner_color = self._winner_color(match.snapshot)
        if winner_color is None:
            logger.warning(
                "rating_update_skipped room_id=%s "
                "reason=winner_undetermined",
                match.room.room_id,
            )
            return
        white = match.room.white
        black = match.room.black
        if white is None or black is None:
            return
        winner_username, loser_username = (
            (white.username, black.username)
            if winner_color == "white"
            else (black.username, white.username)
        )
        await asyncio.to_thread(
            self._persist_rating_update,
            winner_username,
            loser_username,
        )

    def _persist_rating_update(
        self,
        winner_username: str,
        loser_username: str,
    ) -> None:
        assert self._session_factory is not None
        try:
            with self._session_factory() as session:
                winner = session.scalar(
                    select(User).where(
                        User.username == winner_username
                    )
                )
                loser = session.scalar(
                    select(User).where(
                        User.username == loser_username
                    )
                )
                if winner is None or loser is None:
                    logger.warning(
                        "rating_update_skipped winner=%s loser=%s "
                        "reason=user_not_found",
                        winner_username,
                        loser_username,
                    )
                    return
                new_winner_rating, new_loser_rating = update_ratings(
                    winner.rating, loser.rating
                )
                winner.rating = new_winner_rating
                loser.rating = new_loser_rating
                session.commit()
                logger.info(
                    "rating_update winner=%s winner_rating=%s "
                    "loser=%s loser_rating=%s",
                    winner_username,
                    new_winner_rating,
                    loser_username,
                    new_loser_rating,
                )
        except SQLAlchemyError:
            logger.error(
                "rating_update_failed winner=%s loser=%s",
                winner_username,
                loser_username,
            )

    async def cleanup_room(self, room_id: UUID) -> None:
        async with self._lock:
            match = self._matches.pop(room_id, None)
        if match is not None:
            before_start = time.time() < match.game_starts_at
            if match.countdown_task is not None:
                match.countdown_task.cancel()
            if before_start:
                logger.warning("match_cancel room_id=%s", room_id)
                remaining = [
                    match.room.white, match.room.black,
                    *match.room.spectators.values(),
                ]
                await asyncio.gather(*(
                    self._send(
                        participant.websocket,
                        match_cancelled_message(room_id),
                    )
                    for participant in remaining
                    if participant is not None
                ))
            await match.bridge.close()
            await self._allocator.release()
