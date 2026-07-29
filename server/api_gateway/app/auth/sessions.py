import json
import secrets
from datetime import UTC, datetime
from uuid import UUID

from redis import Redis
from redis.exceptions import RedisError


SESSION_PREFIX = "ctd:session:"


class SessionStoreUnavailable(RuntimeError):
    pass


def create_session(
    redis_client: Redis,
    user_id: UUID,
    ttl_seconds: int,
) -> str:
    token = secrets.token_urlsafe(32)
    payload = json.dumps(
        {
            "user_id": str(user_id),
            "created_at": datetime.now(UTC).isoformat(),
        },
        separators=(",", ":"),
    )
    try:
        redis_client.set(
            f"{SESSION_PREFIX}{token}",
            payload,
            ex=ttl_seconds,
        )
    except RedisError as error:
        raise SessionStoreUnavailable from error
    return token


def resolve_session(
    redis_client: Redis,
    token: str,
) -> UUID | None:
    if not token:
        return None
    try:
        raw_payload = redis_client.get(
            f"{SESSION_PREFIX}{token}"
        )
    except RedisError as error:
        raise SessionStoreUnavailable from error
    if raw_payload is None:
        return None
    try:
        if isinstance(raw_payload, bytes):
            raw_payload = raw_payload.decode("utf-8")
        payload = json.loads(raw_payload)
        return UUID(payload["user_id"])
    except (
        UnicodeDecodeError,
        json.JSONDecodeError,
        KeyError,
        TypeError,
        ValueError,
    ):
        return None


def delete_session(redis_client: Redis, token: str) -> None:
    if not token:
        return
    try:
        redis_client.delete(f"{SESSION_PREFIX}{token}")
    except RedisError as error:
        raise SessionStoreUnavailable from error
