import json

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.websocket.authentication import (
    AUTHENTICATION_CLOSE_CODE,
    authenticate_websocket,
)
from app.websocket.schemas import connected_message, error_message


router = APIRouter()


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
            else:
                await websocket.send_json(
                    error_message(
                        "unsupported_message_type",
                        "Message type is not supported",
                    )
                )
    except WebSocketDisconnect:
        return
