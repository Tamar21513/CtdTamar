import logging
from contextlib import asynccontextmanager
from typing import AsyncIterator

import psycopg
import redis
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from app.auth.router import router as auth_router
from app.config import Settings, get_settings
from app.db.session import create_database_engine, create_session_factory
from app.health import build_health_response
from app.logging_config import configure_logging
from app.matches.manager import MatchManager
from app.matches.play_queue import PlayQueue
from app.rooms.manager import RoomManager
from app.websocket.router import router as websocket_router


configure_logging(get_settings())
logger = logging.getLogger(__name__)


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
    # Re-asserted here (not just at module import) because Alembic's
    # `command.upgrade` calls `logging.config.fileConfig`, which
    # detaches every root logger handler, including ours; this
    # re-attaches them whenever the app starts up.
    configure_logging(settings)
    engine = create_database_engine(settings)
    redis_client = create_redis_client(settings)
    app.state.settings = settings
    app.state.db_engine = engine
    app.state.db_session_factory = create_session_factory(engine)
    app.state.redis_client = redis_client
    app.state.room_manager = RoomManager()
    app.state.play_queue = PlayQueue()

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
        session_factory=app.state.db_session_factory,
        additional_shards=settings.game_server_shards,
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


@app.exception_handler(Exception)
async def unhandled_exception_handler(
    request: Request, exc: Exception
) -> JSONResponse:
    # Deliberately logs only method + path, never the request body -
    # the auth endpoints carry a password field in their body.
    logger.exception(
        "unhandled_exception method=%s path=%s",
        request.method,
        request.url.path,
    )
    return JSONResponse(
        status_code=500,
        content={"detail": "Internal server error"},
    )


@app.get("/health")
def health() -> JSONResponse:
    settings = get_settings()
    body, status_code = build_health_response(
        lambda: check_postgres(settings),
        lambda: check_redis(settings),
    )
    return JSONResponse(content=body, status_code=status_code)
