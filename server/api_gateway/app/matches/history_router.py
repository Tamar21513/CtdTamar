from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy import or_, select
from sqlalchemy.orm import Session

from app.auth.dependencies import get_current_user
from app.db.models import MatchHistory, User
from app.db.session import get_db_session
from app.matches.history_schemas import MatchHistoryEntry


router = APIRouter(prefix="/matches", tags=["matches"])

HISTORY_LIMIT = 50


@router.get("/history", response_model=list[MatchHistoryEntry])
def get_match_history(
    user: Annotated[User, Depends(get_current_user)],
    session: Annotated[Session, Depends(get_db_session)],
) -> list[MatchHistoryEntry]:
    rows = session.scalars(
        select(MatchHistory)
        .where(
            or_(
                MatchHistory.white_user_id == user.id,
                MatchHistory.black_user_id == user.id,
            )
        )
        .order_by(MatchHistory.ended_at.desc())
        .limit(HISTORY_LIMIT)
    ).all()
    entries: list[MatchHistoryEntry] = []
    for row in rows:
        is_white = row.white_user_id == user.id
        color = "white" if is_white else "black"
        opponent = row.black_username if is_white else row.white_username
        rating_before = (
            row.white_rating_before if is_white else row.black_rating_before
        )
        rating_after = (
            row.white_rating_after if is_white else row.black_rating_after
        )
        result = "win" if row.winner_color == color else "loss"
        entries.append(
            MatchHistoryEntry(
                opponent=opponent,
                color=color,
                result=result,
                rating_before=rating_before,
                rating_after=rating_after,
                reason=row.reason,
                ended_at=row.ended_at,
            )
        )
    return entries
