from datetime import datetime

from pydantic import BaseModel


class MatchHistoryEntry(BaseModel):
    opponent: str
    color: str
    result: str
    rating_before: int
    rating_after: int
    reason: str
    ended_at: datetime
