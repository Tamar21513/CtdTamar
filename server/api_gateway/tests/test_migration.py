from alembic import command
from alembic.config import Config
from sqlalchemy import inspect

def test_empty_database_upgrades_to_users_schema(client) -> None:
    config = Config("alembic.ini")
    command.downgrade(config, "base")
    assert "users" not in inspect(
        client.app.state.db_engine
    ).get_table_names()
    command.upgrade(config, "head")

    inspector = inspect(client.app.state.db_engine)
    assert "users" in inspector.get_table_names()
    unique_constraints = inspector.get_unique_constraints("users")
    assert any(
        constraint["name"] == "uq_users_username_normalized"
        for constraint in unique_constraints
    )
    indexes = inspector.get_indexes("users")
    assert any(
        index["name"] == "ix_users_username_normalized"
        for index in indexes
    )
