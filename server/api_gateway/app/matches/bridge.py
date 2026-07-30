import asyncio
import json
from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any


BridgeEventHandler = Callable[[str, dict[str, Any]], Awaitable[None]]


def _position(row: int = -1, col: int = -1) -> dict[str, int]:
    return {"row": row, "col": col}


def _protocol_message(
    message_type: str,
    *,
    sequence: int = 0,
    username: str = "",
    source: dict[str, int] | None = None,
    destination: dict[str, int] | None = None,
) -> dict[str, Any]:
    return {
        "version": 5,
        "type": message_type,
        "sequence": sequence,
        "source": source or _position(),
        "destination": destination or _position(),
        "accepted": False,
        "reason": "",
        "playerColor": "",
        "reconnectToken": "",
        "username": username,
        "whiteUsername": "",
        "blackUsername": "",
        "secondsRemaining": 0,
        "createdAtMs": 0,
        "hasSnapshot": False,
    }


@dataclass
class _PlayerConnection:
    reader: asyncio.StreamReader
    writer: asyncio.StreamWriter
    task: asyncio.Task[None] | None = None


class GameServerBridge:
    """Translates routing messages without implementing chess rules."""

    def __init__(self, host: str, port: int) -> None:
        self._host = host
        self._port = port
        self._players: dict[str, _PlayerConnection] = {}
        self._handler: BridgeEventHandler | None = None

    async def _connect_player(
        self,
        color: str,
        username: str,
    ) -> list[dict[str, Any]]:
        reader, writer = await asyncio.open_connection(
            self._host, self._port
        )
        writer.write(
            (
                json.dumps(
                    _protocol_message(
                        "reconnect_request",
                        username=username,
                    ),
                    separators=(",", ":"),
                )
                + "\n"
            ).encode()
        )
        await writer.drain()
        self._players[color] = _PlayerConnection(reader, writer)

        received: list[dict[str, Any]] = []
        while True:
            raw = await asyncio.wait_for(
                reader.readline(), timeout=5.0
            )
            if not raw:
                raise ConnectionError(
                    "Authoritative game server disconnected during startup."
                )
            message = json.loads(raw)
            received.append(message)
            if message.get("type") in {
                "waiting_for_opponent",
                "game_started",
                "game_full",
                "invalid_username",
            }:
                return received

    async def start(
        self,
        white_username: str,
        black_username: str,
        handler: BridgeEventHandler,
    ) -> dict[str, Any]:
        self._handler = handler
        try:
            white_messages = await self._connect_player(
                "white", white_username
            )
            black_messages = await self._connect_player(
                "black", black_username
            )
            if black_messages[-1].get("type") != "game_started":
                raise ConnectionError(
                    "Authoritative server did not start the match."
                )

            white = self._players["white"]
            while not any(
                item.get("type") == "game_started"
                for item in white_messages
            ):
                raw = await asyncio.wait_for(
                    white.reader.readline(), timeout=5.0
                )
                if not raw:
                    raise ConnectionError(
                        "White connection closed during startup."
                    )
                white_messages.append(json.loads(raw))

            started = next(
                item
                for item in reversed(black_messages + white_messages)
                if item.get("type") == "game_started"
                and item.get("hasSnapshot")
            )
            for color, connection in self._players.items():
                connection.task = asyncio.create_task(
                    self._read_loop(color, connection)
                )
            return started["snapshot"]
        except Exception:
            await self.close()
            raise

    async def _read_loop(
        self,
        color: str,
        connection: _PlayerConnection,
    ) -> None:
        try:
            while raw := await connection.reader.readline():
                message = json.loads(raw)
                if self._handler is not None:
                    await self._handler(color, message)
        except (ConnectionError, json.JSONDecodeError):
            return

    async def send_move(
        self,
        color: str,
        sequence: int,
        source: dict[str, int],
        destination: dict[str, int],
    ) -> None:
        await self._send_action(
            color,
            "move_request",
            sequence,
            source,
            destination,
        )

    async def send_jump(
        self,
        color: str,
        sequence: int,
        cell: dict[str, int],
    ) -> None:
        await self._send_action(
            color,
            "jump_request",
            sequence,
            cell,
            cell,
        )

    async def _send_action(
        self,
        color: str,
        message_type: str,
        sequence: int,
        source: dict[str, int],
        destination: dict[str, int],
    ) -> None:
        connection = self._players[color]
        message = _protocol_message(
            message_type,
            sequence=sequence,
            source=source,
            destination=destination,
        )
        connection.writer.write(
            (
                json.dumps(message, separators=(",", ":"))
                + "\n"
            ).encode()
        )
        await connection.writer.drain()

    async def close(self) -> None:
        tasks: list[asyncio.Task[None]] = []
        for connection in self._players.values():
            if connection.task is not None:
                connection.task.cancel()
                tasks.append(connection.task)
            connection.writer.close()
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)
        for connection in self._players.values():
            try:
                await connection.writer.wait_closed()
            except (ConnectionError, RuntimeError):
                pass
        self._players.clear()
