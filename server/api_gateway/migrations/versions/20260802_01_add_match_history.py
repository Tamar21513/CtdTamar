"""Add match_history table.

Revision ID: 20260802_01
Revises: 20260731_01
"""
from typing import Sequence

import sqlalchemy as sa
from alembic import op
from sqlalchemy.dialects import postgresql


revision: str = "20260802_01"
down_revision: str | None = "20260731_01"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "match_history",
        sa.Column(
            "id", postgresql.UUID(as_uuid=True), primary_key=True
        ),
        sa.Column("room_id", postgresql.UUID(as_uuid=True), nullable=False),
        sa.Column(
            "white_user_id",
            postgresql.UUID(as_uuid=True),
            sa.ForeignKey("users.id"),
            nullable=False,
        ),
        sa.Column(
            "black_user_id",
            postgresql.UUID(as_uuid=True),
            sa.ForeignKey("users.id"),
            nullable=False,
        ),
        sa.Column("white_username", sa.String(32), nullable=False),
        sa.Column("black_username", sa.String(32), nullable=False),
        sa.Column("winner_color", sa.String(8), nullable=False),
        sa.Column("white_rating_before", sa.Integer(), nullable=False),
        sa.Column("white_rating_after", sa.Integer(), nullable=False),
        sa.Column("black_rating_before", sa.Integer(), nullable=False),
        sa.Column("black_rating_after", sa.Integer(), nullable=False),
        sa.Column("reason", sa.String(32), nullable=False),
        sa.Column(
            "ended_at",
            sa.DateTime(timezone=True),
            server_default=sa.text("now()"),
            nullable=False,
        ),
    )


def downgrade() -> None:
    op.drop_table("match_history")
