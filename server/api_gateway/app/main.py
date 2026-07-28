import os

import psycopg
import redis
from fastapi import FastAPI
from fastapi.responses import JSONResponse

from app.health import build_health_response


app = FastAPI(
    title="CTD API Gateway",
    docs_url=None,
    redoc_url=None,
    openapi_url=None,
)


def check_postgres() -> bool:
    try:
        with psycopg.connect(
            dbname=os.environ["POSTGRES_DB"],
            user=os.environ["POSTGRES_USER"],
            password=os.environ["POSTGRES_PASSWORD"],
            host=os.environ["POSTGRES_HOST"],
            port=int(os.environ["POSTGRES_PORT"]),
            connect_timeout=2,
        ) as connection:
            with connection.cursor() as cursor:
                cursor.execute("SELECT 1")
                return cursor.fetchone() == (1,)
    except (KeyError, ValueError, psycopg.Error):
        return False


def check_redis() -> bool:
    try:
        client = redis.Redis(
            host=os.environ["REDIS_HOST"],
            port=int(os.environ["REDIS_PORT"]),
            socket_connect_timeout=2,
            socket_timeout=2,
        )
        return bool(client.ping())
    except (KeyError, ValueError, redis.RedisError):
        return False


@app.get("/health")
def health() -> JSONResponse:
    body, status_code = build_health_response(
        check_postgres,
        check_redis,
    )
    return JSONResponse(
        content=body,
        status_code=status_code,
    )
