from functools import lru_cache
from typing import Literal

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=None,
        case_sensitive=True,
        extra="ignore",
    )

    POSTGRES_DB: str = "ctd"
    POSTGRES_USER: str = "ctd"
    POSTGRES_PASSWORD: str
    POSTGRES_HOST: str = "postgres"
    POSTGRES_PORT: int = Field(default=5432, ge=1, le=65535)

    REDIS_HOST: str = "redis"
    REDIS_PORT: int = Field(default=6379, ge=1, le=65535)
    REDIS_DB: int = Field(default=0, ge=0)

    API_GATEWAY_PORT: int = Field(default=8000, ge=1, le=65535)
    CTD_GAME_SERVER_HOST: str = "host.docker.internal"
    CTD_GAME_SERVER_PORT: int = Field(
        default=5050, ge=1, le=65535
    )
    CTD_GAME_SERVER_SHARDS: str = ""

    @property
    def game_server_shards(self) -> list[tuple[str, int]]:
        if not self.CTD_GAME_SERVER_SHARDS.strip():
            return []
        shards: list[tuple[str, int]] = []
        for entry in self.CTD_GAME_SERVER_SHARDS.split(","):
            entry = entry.strip()
            if not entry:
                continue
            host, _, port_text = entry.rpartition(":")
            shards.append((host, int(port_text)))
        return shards

    SESSION_COOKIE_NAME: str = "ctd_session"
    SESSION_TTL_SECONDS: int = Field(default=3600, ge=60)
    SESSION_COOKIE_SECURE: bool = False
    SESSION_COOKIE_SAMESITE: Literal["lax", "strict", "none"] = "lax"

    LOG_DIR: str = "/app/logs"

    @property
    def database_url(self) -> str:
        from urllib.parse import quote_plus

        return (
            "postgresql+psycopg://"
            f"{quote_plus(self.POSTGRES_USER)}:"
            f"{quote_plus(self.POSTGRES_PASSWORD)}@"
            f"{self.POSTGRES_HOST}:{self.POSTGRES_PORT}/"
            f"{quote_plus(self.POSTGRES_DB)}"
        )


@lru_cache
def get_settings() -> Settings:
    return Settings()
