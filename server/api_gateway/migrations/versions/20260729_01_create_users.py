"""Create users table.

Revision ID: 20260729_01
Revises:
"""
from typing import Sequence

import sqlalchemy as sa
from alembic import op
from sqlalchemy.dialects import postgresql


revision: str = "20260729_01"
down_revision: str | None = None
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "users",
        sa.Column(
            "id",
            postgresql.UUID(as_uuid=True),
            nullable=False,
        ),
        sa.Column("username", sa.String(length=32), nullable=False),
        sa.Column(
            "username_normalized",
            sa.String(length=32),
            nullable=False,
        ),
        sa.Column(
            "password_hash",
            sa.String(length=512),
            nullable=False,
        ),
        sa.Column(
            "created_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.Column(
            "updated_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
        sa.PrimaryKeyConstraint("id", name="pk_users"),
        sa.UniqueConstraint(
            "username_normalized",
            name="uq_users_username_normalized",
        ),
    )
    op.create_index(
        "ix_users_username_normalized",
        "users",
        ["username_normalized"],
        unique=True,
    )


def downgrade() -> None:
    op.drop_index(
        "ix_users_username_normalized",
        table_name="users",
    )
    op.drop_table("users")
