from sqlalchemy import select
from sqlalchemy.exc import IntegrityError, SQLAlchemyError
from sqlalchemy.orm import Session

from app.auth.passwords import hash_password, verify_password
from app.auth.schemas import CredentialsRequest, normalize_username
from app.db.models import User


class DuplicateUsernameError(RuntimeError):
    pass


class AuthenticationFailed(RuntimeError):
    pass


class AuthenticationInfrastructureError(RuntimeError):
    pass


def register_user(
    session: Session,
    credentials: CredentialsRequest,
) -> User:
    normalized = normalize_username(credentials.username)
    existing = session.scalar(
        select(User).where(
            User.username_normalized == normalized
        )
    )
    if existing is not None:
        raise DuplicateUsernameError

    user = User(
        username=credentials.username,
        username_normalized=normalized,
        password_hash=hash_password(
            credentials.password.get_secret_value()
        ),
    )
    session.add(user)
    try:
        session.commit()
        session.refresh(user)
    except IntegrityError as error:
        session.rollback()
        raise DuplicateUsernameError from error
    except SQLAlchemyError as error:
        session.rollback()
        raise AuthenticationInfrastructureError from error
    return user


def authenticate_user(
    session: Session,
    credentials: CredentialsRequest,
) -> User:
    normalized = normalize_username(credentials.username)
    try:
        user = session.scalar(
            select(User).where(
                User.username_normalized == normalized
            )
        )
    except SQLAlchemyError as error:
        raise AuthenticationInfrastructureError from error

    if user is None or not verify_password(
        user.password_hash,
        credentials.password.get_secret_value(),
    ):
        raise AuthenticationFailed
    return user
