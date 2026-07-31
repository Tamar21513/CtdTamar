"""Pure ELO rating math. No database or FastAPI dependency."""

K_FACTOR = 32


def expected_score(rating_a: int, rating_b: int) -> float:
    """Probability that the side rated `rating_a` beats `rating_b`."""
    return 1.0 / (1.0 + 10 ** ((rating_b - rating_a) / 400.0))


def update_ratings(
    winner_rating: int,
    loser_rating: int,
    k: int = K_FACTOR,
) -> tuple[int, int]:
    """Returns (new_winner_rating, new_loser_rating) after one game.

    Standard ELO: each side's rating shifts by k * (actual - expected),
    where actual is 1 for the winner and 0 for the loser. Both results
    are rounded to the nearest integer with round() (Python's
    round-half-to-even), applied consistently to both sides.
    """
    winner_expected = expected_score(winner_rating, loser_rating)
    loser_expected = expected_score(loser_rating, winner_rating)
    new_winner_rating = winner_rating + k * (1 - winner_expected)
    new_loser_rating = loser_rating + k * (0 - loser_expected)
    return round(new_winner_rating), round(new_loser_rating)
