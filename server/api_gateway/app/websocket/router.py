import json
from uuid import UUID

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.matches.manager import MatchManager, MatchOperationError
from app.rooms.manager import RoomManager, RoomOperationError
from app.rooms.schemas import (
    error_message as room_error_message,
    room_created_message,
    watching_game_message,
)
from app.websocket.authentication import (
    AUTHENTICATION_CLOSE_CODE,
    authenticate_websocket,
)
from app.websocket.schemas import connected_message, error_message


router = APIRouter()


async def _safe_send(
    websocket: WebSocket,
    message: dict,
) -> bool:
    try:
        await websocket.send_json(message)
        return True
    except (WebSocketDisconnect, RuntimeError):
        return False


async def _broadcast_lobby(manager: RoomManager) -> None:
    subscribers, snapshot = await manager.lobby_delivery()
    for subscriber in subscribers:
        await _safe_send(subscriber, snapshot)


def _room_uuid(message: dict) -> UUID:
    try:
        return UUID(str(message.get("room_id", "")))
    except ValueError as error:
        raise RoomOperationError(
            "invalid_room_id",
            "Room ID must be a valid UUID.",
        ) from error


async def _handle_room_message(
    websocket: WebSocket,
    user,
    message: dict,
) -> bool:
    manager: RoomManager = websocket.app.state.room_manager
    matches: MatchManager = websocket.app.state.match_manager
    message_type = message.get("type")
    try:
        if message_type == "create_room":
            room = await manager.create_room(
                user.id,
                user.username,
                websocket,
                message.get("name"),
                str(message.get("visibility", "")),
            )
            await websocket.send_json(room_created_message(room))
            if room.visibility == "public":
                await _broadcast_lobby(manager)
            return True

        if message_type == "get_lobby":
            await websocket.send_json(await manager.lobby_snapshot())
            return True

        if message_type == "subscribe_lobby":
            await websocket.send_json(
                await manager.subscribe_lobby(websocket)
            )
            return True

        if message_type == "join_room":
            room = await manager.join_public_room(
                _room_uuid(message),
                user.id,
                user.username,
                websocket,
            )
            try:
                await matches.start_match(room)
            except MatchOperationError:
                await manager.rollback_join(room.room_id, user.id)
                raise
            await _broadcast_lobby(manager)
            return True

        if message_type == "join_hidden_room":
            room_code = message.get("room_code")
            if not isinstance(room_code, str) or not room_code.strip():
                raise RoomOperationError(
                    "invalid_room_code",
                    "Room code is invalid or no longer available.",
                )
            room = await manager.join_hidden_room(
                room_code,
                user.id,
                user.username,
                websocket,
            )
            try:
                await matches.start_match(room)
            except MatchOperationError:
                await manager.rollback_join(room.room_id, user.id)
                raise
            await _broadcast_lobby(manager)
            return True

        if message_type == "watch_game":
            room = await manager.watch_room(
                _room_uuid(message),
                user.id,
                user.username,
                websocket,
            )
            await websocket.send_json(watching_game_message(room))
            await matches.watch_match(room.room_id, websocket)
            await _broadcast_lobby(manager)
            return True
        if message_type == "leave_spectator":
            await manager.leave_spectator(
                websocket, _room_uuid(message)
            )
            await websocket.send_json(
                {
                    "type": "spectator_left",
                    "room_id": str(_room_uuid(message)),
                }
            )
            await _broadcast_lobby(manager)
            return True
        if message_type == "move_request":
            await matches.move(
                _room_uuid(message),
                user.id,
                websocket,
                message.get("sequence"),
                message.get("from"),
                message.get("to"),
            )
            return True
        if message_type == "jump_request":
            await matches.jump(
                _room_uuid(message),
                user.id,
                websocket,
                message.get("sequence"),
                message.get("cell"),
            )
            return True
    except (RoomOperationError, MatchOperationError) as error:
        await websocket.send_json(
            room_error_message(error.code, error.message)
        )
        return True
    return False


@router.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket) -> None:
    user = authenticate_websocket(
        websocket=websocket,
        settings=websocket.app.state.settings,
        session_factory=websocket.app.state.db_session_factory,
        redis_client=websocket.app.state.redis_client,
    )
    if user is None:
        await websocket.close(code=AUTHENTICATION_CLOSE_CODE)
        return

    await websocket.accept()
    await websocket.send_json(
        connected_message(str(user.id), user.username)
    )

    try:
        while True:
            raw_message = await websocket.receive_text()
            try:
                message = json.loads(raw_message)
            except json.JSONDecodeError:
                await websocket.send_json(
                    error_message(
                        "invalid_json",
                        "Message must be valid JSON",
                    )
                )
                continue

            if not isinstance(message, dict):
                await websocket.send_json(
                    error_message(
                        "invalid_message",
                        "Message must be a JSON object",
                    )
                )
            elif message.get("type") == "ping":
                await websocket.send_json({"type": "pong"})
            elif await _handle_room_message(
                websocket,
                user,
                message,
            ):
                continue
            else:
                await websocket.send_json(
                    error_message(
                        "unsupported_message_type",
                        "Message type is not supported",
                    )
                )
    except WebSocketDisconnect:
        return
    finally:
        manager: RoomManager = websocket.app.state.room_manager
        cleanup = await manager.remove_connection(
            user.id,
            websocket,
        )
        if cleanup.removed_room_id is not None:
            matches: MatchManager = websocket.app.state.match_manager
            await matches.cleanup_room(cleanup.removed_room_id)
        for notification in cleanup.notifications:
            await _safe_send(
                notification.websocket,
                notification.message,
            )
        if cleanup.lobby_changed:
            await _broadcast_lobby(manager)
