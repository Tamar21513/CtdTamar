"""Add rating column to users.

Revision ID: 20260731_01
Revises: 20260729_01
"""
from typing import Sequence

import sqlalchemy as sa
from alembic import op


revision: str = "20260731_01"
down_revision: str | None = "20260729_01"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column(
        "users",
        sa.Column(
            "rating",
            sa.Integer(),
            server_default=sa.text("1200"),
            nullable=False,
        ),
    )


def downgrade() -> None:
    op.drop_column("users", "rating")
