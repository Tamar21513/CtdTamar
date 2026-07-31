from app.matches.rating import K_FACTOR, expected_score, update_ratings


def test_k_factor_is_32() -> None:
    assert K_FACTOR == 32


def test_expected_score_of_equal_ratings_is_half() -> None:
    assert expected_score(1200, 1200) == 0.5


def test_equal_ratings_produce_symmetric_change() -> None:
    winner, loser = update_ratings(1200, 1200)
    assert (winner, loser) == (1216, 1184)


def test_strong_player_beating_weak_player_gains_little() -> None:
    winner, loser = update_ratings(1600, 1200)
    assert (winner, loser) == (1603, 1197)
    assert winner - 1600 == 3


def test_weak_player_beating_strong_player_gains_a_lot() -> None:
    winner, loser = update_ratings(1200, 1600)
    assert (winner, loser) == (1229, 1571)
    assert winner - 1200 == 29


def test_rating_changes_are_approximately_zero_sum() -> None:
    pairs = [
        (1200, 1200),
        (1600, 1200),
        (1200, 1600),
        (1400, 1300),
        (2000, 800),
        (900, 1100),
    ]
    for winner_rating, loser_rating in pairs:
        new_winner, new_loser = update_ratings(
            winner_rating, loser_rating
        )
        winner_gain = new_winner - winner_rating
        loser_loss = loser_rating - new_loser
        # Both sides move by the same real-valued amount before
        # rounding; independent integer rounding can only disagree
        # by at most 1.
        assert abs(winner_gain - loser_loss) <= 1
