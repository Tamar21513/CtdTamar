from contextlib import asynccontextmanager
from typing import AsyncIterator

import psycopg
import redis
from fastapi import FastAPI
from fastapi.responses import JSONResponse

from app.auth.router import router as auth_router
from app.config import Settings, get_settings
from app.db.session import create_database_engine, create_session_factory
from app.health import build_health_response
from app.matches.manager import MatchManager
from app.rooms.manager import RoomManager
from app.websocket.router import router as websocket_router


def check_postgres(settings: Settings | None = None) -> bool:
    try:
        current = settings or get_settings()
        with psycopg.connect(
            dbname=current.POSTGRES_DB,
            user=current.POSTGRES_USER,
            password=current.POSTGRES_PASSWORD,
            host=current.POSTGRES_HOST,
            port=current.POSTGRES_PORT,
            connect_timeout=2,
        ) as connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT 1")
                return cursor.fetchone() == (1,)
    except (KeyError, ValueError, psycopg.Error):
        return False


def create_redis_client(settings: Settings) -> redis.Redis:
    return redis.Redis(
        host=settings.REDIS_HOST,
        port=settings.REDIS_PORT,
        db=settings.REDIS_DB,
        socket_connect_timeout=2,
        socket_timeout=2,
    )


def check_redis(settings: Settings | None = None) -> bool:
    client: redis.Redis | None = None
    try:
        current = settings or get_settings()
        client = create_redis_client(current)
        return bool(client.ping())
    except (KeyError, ValueError, redis.RedisError):
        return False
    finally:
        if client is not None:
            client.close()


@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    settings = get_settings()
    engine = create_database_engine(settings)
    redis_client = create_redis_client(settings)
    app.state.settings = settings
    app.state.db_engine = engine
    app.state.db_session_factory = create_session_factory(engine)
    app.state.redis_client = redis_client
    app.state.room_manager = RoomManager()

    async def match_ended(room_id) -> None:
        await app.state.room_manager.finish_room(room_id)
        subscribers, snapshot = (
            await app.state.room_manager.lobby_delivery()
        )
        for websocket in subscribers:
            try:
                await websocket.send_json(snapshot)
            except RuntimeError:
                pass

    app.state.match_manager = MatchManager(
        settings.CTD_GAME_SERVER_HOST,
        settings.CTD_GAME_SERVER_PORT,
        match_ended_handler=match_ended,
    )
    try:
        yield
    finally:
        redis_client.close()
        engine.dispose()


app = FastAPI(
    title="CTD API Gateway",
    docs_url=None,
    redoc_url=None,
    openapi_url=None,
    lifespan=lifespan,
)
app.include_router(auth_router)
app.include_router(websocket_router)


@app.get("/health")
def health() -> JSONResponse:
    settings = get_settings()
    body, status_code = build_health_response(
        lambda: check_postgres(settings),
        lambda: check_redis(settings),
    )
    return JSONResponse(content=body, status_code=status_code)
