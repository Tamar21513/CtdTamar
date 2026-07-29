from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Request, Response, status
from redis import Redis
from sqlalchemy.orm import Session

from app.auth.dependencies import get_current_user, get_redis_client
from app.auth.schemas import CredentialsRequest, UserResponse
from app.auth.service import (
    AuthenticationFailed,
    AuthenticationInfrastructureError,
    DuplicateUsernameError,
    authenticate_user,
    register_user,
)
from app.auth.sessions import (
    SessionStoreUnavailable,
    create_session,
    delete_session,
)
from app.config import Settings, get_settings
from app.db.models import User
from app.db.session import get_db_session


router = APIRouter(prefix="/auth", tags=["authentication"])


def _set_session_cookie(
    response: Response,
    token: str,
    settings: Settings,
) -> None:
    response.set_cookie(
        key=settings.SESSION_COOKIE_NAME,
        value=token,
        max_age=settings.SESSION_TTL_SECONDS,
        httponly=True,
        secure=settings.SESSION_COOKIE_SECURE,
        samesite=settings.SESSION_COOKIE_SAMESITE,
        path="/",
    )


def _clear_session_cookie(
    response: Response,
    settings: Settings,
) -> None:
    response.delete_cookie(
        key=settings.SESSION_COOKIE_NAME,
        httponly=True,
        secure=settings.SESSION_COOKIE_SECURE,
        samesite=settings.SESSION_COOKIE_SAMESITE,
        path="/",
    )


@router.post(
    "/register",
    response_model=UserResponse,
    status_code=status.HTTP_201_CREATED,
)
def register(
    credentials: CredentialsRequest,
    session: Annotated[Session, Depends(get_db_session)],
) -> User:
    try:
        return register_user(session, credentials)
    except DuplicateUsernameError as error:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Username is already registered",
        ) from error
    except AuthenticationInfrastructureError as error:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Authentication service unavailable",
        ) from error


@router.post("/login", response_model=UserResponse)
def login(
    credentials: CredentialsRequest,
    response: Response,
    session: Annotated[Session, Depends(get_db_session)],
    redis_client: Annotated[Redis, Depends(get_redis_client)],
    settings: Annotated[Settings, Depends(get_settings)],
) -> User:
    try:
        user = authenticate_user(session, credentials)
        token = create_session(
            redis_client,
            user.id,
            settings.SESSION_TTL_SECONDS,
        )
    except AuthenticationFailed as error:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid username or password",
        ) from error
    except (
        AuthenticationInfrastructureError,
        SessionStoreUnavailable,
    ) as error:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Authentication service unavailable",
        ) from error
    _set_session_cookie(response, token, settings)
    return user


@router.get("/me", response_model=UserResponse)
def me(
    user: Annotated[User, Depends(get_current_user)],
) -> User:
    return user


@router.post("/logout", status_code=status.HTTP_204_NO_CONTENT)
def logout(
    request: Request,
    response: Response,
    redis_client: Annotated[Redis, Depends(get_redis_client)],
    settings: Annotated[Settings, Depends(get_settings)],
) -> None:
    token = request.cookies.get(settings.SESSION_COOKIE_NAME, "")
    try:
        delete_session(redis_client, token)
    except SessionStoreUnavailable:
        pass
    _clear_session_cookie(response, settings)
