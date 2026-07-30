import os
from collections.abc import Iterator

import pytest
from alembic import command
from alembic.config import Config
from fastapi.testclient import TestClient
from sqlalchemy import delete

from app.config import get_settings
from app.db.models import User
from app.main import app
from app.matches.manager import MatchManager


@pytest.fixture(scope="session", autouse=True)
def migrated_database() -> Iterator[None]:
    config = Config("alembic.ini")
    command.upgrade(config, "head")
    yield


@pytest.fixture(autouse=True)
def clean_infrastructure() -> Iterator[None]:
    with TestClient(app) as client:
        with client.app.state.db_session_factory() as session:
            session.execute(delete(User))
            session.commit()
        redis_client = client.app.state.redis_client
        for key in redis_client.scan_iter(match="ctd:session:*"):
            redis_client.delete(key)
    yield
    with TestClient(app) as client:
        with client.app.state.db_session_factory() as session:
            session.execute(delete(User))
            session.commit()
        redis_client = client.app.state.redis_client
        for key in redis_client.scan_iter(match="ctd:session:*"):
            redis_client.delete(key)


@pytest.fixture
def client() -> Iterator[TestClient]:
    with TestClient(app) as test_client:
        class TestBridge:
            def __init__(self, host: str, port: int) -> None:
                self.handler = None

            async def start(self, white, black, handler):
                self.handler = handler
                return {
                    "serverTimeMs": 1,
                    "gameOver": False,
                    "whiteScore": 0,
                    "blackScore": 0,
                    "whiteUsername": white,
                    "blackUsername": black,
                    "playersReady": True,
                    "gameplayStartsAtEpochMs": 1,
                    "pieces": [],
                    "motions": [],
                    "jumps": [],
                    "whiteMoveHistory": [],
                    "blackMoveHistory": [],
                }

            async def send_move(
                self, color, sequence, source, destination
            ):
                return None

            async def send_jump(self, color, sequence, cell):
                return None

            async def close(self):
                return None

        test_client.app.state.match_manager = MatchManager(
            "test", 1, TestBridge
        )
        yield test_client


@pytest.fixture
def registered_user(client: TestClient) -> dict[str, str]:
    credentials = {
        "username": "player_one",
        "password": "StrongPassword123!",
    }
    response = client.post("/auth/register", json=credentials)
    assert response.status_code == 201
    return credentials
