from fastapi import WebSocket
from redis import Redis
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.orm import Session, sessionmaker

from app.auth.sessions import SessionStoreUnavailable, resolve_session
from app.config import Settings
from app.db.models import User


AUTHENTICATION_CLOSE_CODE = 4401


def authenticate_websocket(
    websocket: WebSocket,
    settings: Settings,
    session_factory: sessionmaker[Session],
    redis_client: Redis,
) -> User | None:
    token = websocket.cookies.get(settings.SESSION_COOKIE_NAME, "")
    try:
        user_id = resolve_session(redis_client, token)
        if user_id is None:
            return None
        with session_factory() as session:
            return session.get(User, user_id)
    except (SessionStoreUnavailable, SQLAlchemyError):
        return None
