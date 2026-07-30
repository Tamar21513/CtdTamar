import asyncio
from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field
from typing import Any, Protocol
from uuid import UUID

from fastapi import WebSocket

from app.matches.allocator import (
    MatchUnavailableError,
    SingleMatchAllocator,
)
from app.matches.bridge import GameServerBridge
from app.matches.schemas import (
    match_ready_message,
    match_snapshot_message,
    match_state_message,
    move_result_message,
)
from app.rooms.models import GameRoom


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
    ) -> None:
        self._host = host
        self._port = port
        self._bridge_factory = bridge_factory
        self._match_ended_handler = match_ended_handler
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
            raise MatchOperationError(
                "room_not_ready", "The room needs two players."
            )
        try:
            await self._allocator.acquire()
        except MatchUnavailableError as error:
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
            match = ActiveMatch(room, bridge, 1, snapshot)
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
                ),
            )
        except Exception:
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
            message = match_snapshot_message(
                room_id, match.revision, match.snapshot
            )
        await self._send(websocket, message)

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
        if self._match_ended_handler is not None:
            await self._match_ended_handler(room_id)
        await match.bridge.close()
        await self._allocator.release()

    async def cleanup_room(self, room_id: UUID) -> None:
        async with self._lock:
            match = self._matches.pop(room_id, None)
        if match is not None:
            await match.bridge.close()
            await self._allocator.release()
