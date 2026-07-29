from typing import Annotated

from fastapi import Cookie, Depends, HTTPException, Request, status
from redis import Redis
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.orm import Session

from app.auth.sessions import SessionStoreUnavailable, resolve_session
from app.config import Settings, get_settings
from app.db.models import User
from app.db.session import get_db_session


def get_redis_client(request: Request) -> Redis:
    return request.app.state.redis_client


def get_current_user(
    request: Request,
    session: Annotated[Session, Depends(get_db_session)],
    redis_client: Annotated[Redis, Depends(get_redis_client)],
    settings: Annotated[Settings, Depends(get_settings)],
) -> User:
    token = request.cookies.get(settings.SESSION_COOKIE_NAME, "")
    try:
        user_id = resolve_session(redis_client, token)
        user = session.get(User, user_id) if user_id else None
    except (SessionStoreUnavailable, SQLAlchemyError):
        user = None
    if user is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Not authenticated",
        )
    return user
