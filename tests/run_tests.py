import subprocess
import os
import sys

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

EXE_PATH = os.path.join(ROOT_DIR, "main.exe")

CPP_FILES = [
    "main.cpp",
    os.path.join("src", "Position.cpp"),
    os.path.join("src", "Piece.cpp"),
    os.path.join("src", "Board.cpp"),
    os.path.join("src", "BoardParser.cpp"),
    os.path.join("src", "BoardPrinter.cpp"),
    os.path.join("src", "BoardMapper.cpp"),
    os.path.join("src", "PieceRules.cpp"),
    os.path.join("src", "RuleEngine.cpp"),
    os.path.join("src", "GameEngine.cpp"),
    os.path.join("src", "Controller.cpp"),
    os.path.join("src", "RealTimeArbiter.cpp"),
]

tests = [
    {
        "name": "parse_rectangular_board",
        "input": """Board:
wK . .
. bQ .
Commands:
print board
""",
        "expected": """wK . .
. bQ .
"""
    },
    {
        "name": "reject_unknown_token",
        "input": """Board:
wK xZ .
Commands:
print board
""",
        "expected": """ERROR UNKNOWN_TOKEN
"""
    },
    {
        "name": "reject_row_width_mismatch",
        "input": """Board:
wK . .
. bQ
Commands:
print board
""",
        "expected": """ERROR ROW_WIDTH_MISMATCH
"""
    },
    {
        "name": "click_empty_cell_does_not_select",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 150 150
click 50 50
print board
""",
        "expected": """wK . .
. . .
. . .
"""
    },
    {
        "name": "click_outside_board_is_ignored",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 500 500
print board
""",
        "expected": """wK . .
. . .
. . .
"""
    },
    {
        "name": "king_legal_diagonal",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 150 150
wait 1000
print board
""",
        "expected": """. . .
. wK .
. . .
"""
    },
    {
        "name": "king_illegal_two_cells",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 250 250
wait 3000
print board
""",
        "expected": """wK . .
. . .
. . .
"""
    },
    {
        "name": "king_capture_enemy",
        "input": """Board:
wK . .
. bR .
. . .
Commands:
click 50 50
click 150 150
wait 1000
print board
""",
        "expected": """. . .
. wK .
. . .
"""
    },
    {
        "name": "friendly_piece_as_second_click_is_rejected_and_clears_selection",
        "input": """Board:
wK wR .
. . .
. . .
Commands:
click 50 50
click 150 50
click 250 50
wait 1000
print board
""",
        "expected": """wK wR .
. . .
. . .
"""
    },

    # Rook
    {
        "name": "rook_horizontal_legal",
        "input": """Board:
wR . .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 2000
print board
""",
        "expected": """. . wR
. . .
. . .
"""
    },
    {
        "name": "rook_vertical_legal",
        "input": """Board:
wR . .
. . .
. . .
Commands:
click 50 50
click 50 250
wait 2000
print board
""",
        "expected": """. . .
. . .
wR . .
"""
    },
    {
        "name": "rook_diagonal_illegal",
        "input": """Board:
wR . .
. . .
. . .
Commands:
click 50 50
click 150 150
wait 2000
print board
""",
        "expected": """wR . .
. . .
. . .
"""
    },
    {
        "name": "rook_blocked_by_piece",
        "input": """Board:
wR wN .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 3000
print board
""",
        "expected": """wR wN .
. . .
. . .
"""
    },
    {
        "name": "rook_capture_enemy_at_destination",
        "input": """Board:
wR . bN
. . .
. . .
Commands:
click 50 50
click 250 50
wait 2000
print board
""",
        "expected": """. . wR
. . .
. . .
"""
    },

    # Bishop
    {
        "name": "bishop_diagonal_legal",
        "input": """Board:
wB . .
. . .
. . .
Commands:
click 50 50
click 250 250
wait 2000
print board
""",
        "expected": """. . .
. . .
. . wB
"""
    },
    {
        "name": "bishop_straight_illegal",
        "input": """Board:
wB . .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 3000
print board
""",
        "expected": """wB . .
. . .
. . .
"""
    },
    {
        "name": "bishop_blocked_by_piece",
        "input": """Board:
wB . .
. wP .
. . .
Commands:
click 50 50
click 250 250
wait 3000
print board
""",
        "expected": """wB . .
. wP .
. . .
"""
    },
    {
        "name": "bishop_capture_enemy",
        "input": """Board:
wB . .
. . .
. . bR
Commands:
click 50 50
click 250 250
wait 2000
print board
""",
        "expected": """. . .
. . .
. . wB
"""
    },

    # Queen
    {
        "name": "queen_horizontal_legal",
        "input": """Board:
wQ . .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 2000
print board
""",
        "expected": """. . wQ
. . .
. . .
"""
    },
    {
        "name": "queen_diagonal_legal",
        "input": """Board:
wQ . .
. . .
. . .
Commands:
click 50 50
click 250 250
wait 2000
print board
""",
        "expected": """. . .
. . .
. . wQ
"""
    },
    {
        "name": "queen_knight_shape_illegal",
        "input": """Board:
wQ . .
. . .
. . .
Commands:
click 50 50
click 150 250
wait 3000
print board
""",
        "expected": """wQ . .
. . .
. . .
"""
    },
    {
        "name": "queen_blocked_straight",
        "input": """Board:
wQ wP .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 3000
print board
""",
        "expected": """wQ wP .
. . .
. . .
"""
    },
    {
        "name": "queen_blocked_diagonal",
        "input": """Board:
wQ . .
. wP .
. . .
Commands:
click 50 50
click 250 250
wait 3000
print board
""",
        "expected": """wQ . .
. wP .
. . .
"""
    },

    # Knight
    {
        "name": "knight_legal_jump",
        "input": """Board:
wN wP .
. . .
. . .
Commands:
click 50 50
click 150 250
wait 2000
print board
""",
        "expected": """. wP .
. . .
. wN .
"""
    },
    {
        "name": "knight_illegal_diagonal",
        "input": """Board:
wN . .
. . .
. . .
Commands:
click 50 50
click 150 150
wait 2000
print board
""",
        "expected": """wN . .
. . .
. . .
"""
    },
    {
        "name": "knight_capture_enemy",
        "input": """Board:
wN . .
. . .
. bR .
Commands:
click 50 50
click 150 250
wait 2000
print board
""",
        "expected": """. . .
. . .
. wN .
"""
    },

    # Pawns
    {
        "name": "white_pawn_single_step",
        "input": """Board:
. . .
. . .
. wP .
. . .
Commands:
click 150 250
click 150 150
wait 1000
print board
""",
        "expected": """. . .
. wP .
. . .
. . .
"""
    },
    {
        "name": "black_pawn_single_step",
        "input": """Board:
. . .
. bP .
. . .
. . .
Commands:
click 150 150
click 150 250
wait 1000
print board
""",
        "expected": """. . .
. . .
. bP .
. . .
"""
    },
    {
        "name": "white_pawn_cannot_move_down",
        "input": """Board:
. . .
. wP .
. . .
Commands:
click 150 150
click 150 250
wait 1000
print board
""",
        "expected": """. . .
. wP .
. . .
"""
    },
    {
        "name": "black_pawn_cannot_move_up",
        "input": """Board:
. . .
. bP .
. . .
Commands:
click 150 150
click 150 50
wait 1000
print board
""",
        "expected": """. . .
. bP .
. . .
"""
    },
{
    "name": "pawn_cannot_move_two_cells_when_blocked",
    "input": """Board:
. . .
. bN .
. wP .
. . .
Commands:
click 150 250
click 150 50
wait 2000
print board
""",
    "expected": """. . .
. bN .
. wP .
. . .
"""
},
    {
        "name": "pawn_cannot_capture_forward",
        "input": """Board:
. bR .
. wP .
. . .
Commands:
click 150 150
click 150 50
wait 1000
print board
""",
        "expected": """. bR .
. wP .
. . .
"""
    },
    {
        "name": "white_pawn_diagonal_capture_left",
        "input": """Board:
. . .
bR . .
. wP .
. . .
Commands:
click 150 250
click 50 150
wait 1000
print board
""",
        "expected": """. . .
wP . .
. . .
. . .
"""
    },
    {
        "name": "white_pawn_diagonal_capture_right",
        "input": """Board:
. . .
. . bR
. wP .
. . .
Commands:
click 150 250
click 250 150
wait 1000
print board
""",
        "expected": """. . .
. . wP
. . .
. . .
"""
    },
    {
        "name": "black_pawn_diagonal_capture_left",
        "input": """Board:
. . .
. bP .
wR . .
. . .
Commands:
click 150 150
click 50 250
wait 1000
print board
""",
        "expected": """. . .
. . .
bP . .
. . .
"""
    },
    {
        "name": "black_pawn_diagonal_capture_right",
        "input": """Board:
. . .
. bP .
. . wR
. . .
Commands:
click 150 150
click 250 250
wait 1000
print board
""",
        "expected": """. . .
. . .
. . bP
. . .
"""
    },
    {
        "name": "pawn_cannot_diagonal_to_empty",
        "input": """Board:
. . .
. wP .
. . .
Commands:
click 150 150
click 50 50
wait 1000
print board
""",
        "expected": """. . .
. wP .
. . .
"""
    },
    {
        "name": "pawn_cannot_capture_own_color",
        "input": """Board:
wR . .
. wP .
. . .
Commands:
click 150 150
click 50 50
wait 1000
print board
""",
        "expected": """wR . .
. wP .
. . .
"""
    },

    # General and parsing edge cases
    {
        "name": "same_cell_move_ignored",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 50 50
wait 1000
print board
""",
        "expected": """wK . .
. . .
. . .
"""
    },
    {
        "name": "trim_board_header_with_space",
        "input": """ Board:
wK . .
. . .
. . .
Commands:
print board
""",
        "expected": """wK . .
. . .
. . .
"""
    },

    # Movement over time
    {
        "name": "movement_not_arrived_yet_piece_stays_in_origin",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 150 150
wait 999
print board
""",
        "expected": """wK . .
. . .
. . .
"""
    },
    {
        "name": "movement_arrives_after_exact_time",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 150 150
wait 1000
print board
""",
        "expected": """. . .
. wK .
. . .
"""
    },
    {
        "name": "movement_before_and_after_arrival_two_prints",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 150 150
print board
wait 999
print board
wait 1
print board
""",
        "expected": """wK . .
. . .
. . .
wK . .
. . .
. . .
. . .
. wK .
. . .
"""
    },
    {
        "name": "movement_wait_split_into_parts",
        "input": """Board:
wR . .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 400
print board
wait 600
print board
wait 1000
print board
""",
        "expected": """wR . .
. . .
. . .
wR . .
. . .
. . .
. . wR
. . .
. . .
"""
    },
    {
        "name": "movement_after_more_than_required_time",
        "input": """Board:
wB . .
. . .
. . .
Commands:
click 50 50
click 250 250
wait 2500
print board
""",
        "expected": """. . .
. . .
. . wB
"""
    },
    {
        "name": "illegal_move_does_not_create_delayed_move",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 50 250
wait 2000
print board
""",
        "expected": """wK . .
. . .
. . .
"""
    },
    {
        "name": "blocked_move_does_not_create_delayed_move",
        "input": """Board:
wR wN .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 2000
print board
""",
        "expected": """wR wN .
. . .
. . .
"""
    },
    {
        "name": "pawn_move_is_delayed",
        "input": """Board:
. . .
. . .
. wP .
. . .
Commands:
click 150 250
click 150 150
wait 999
print board
wait 1
print board
""",
        "expected": """. . .
. . .
. wP .
. . .
. . .
. wP .
. . .
. . .
"""
    },
    {
        "name": "delayed_capture_happens_only_on_arrival",
        "input": """Board:
wK . .
. bR .
. . .
Commands:
click 50 50
click 150 150
wait 999
print board
wait 1
print board
""",
        "expected": """wK . .
. bR .
. . .
. . .
. wK .
. . .
"""
    },

    # Updated for current rule: only one active move is allowed at a time.
    {
        "name": "multiple_moves_are_not_allowed_concurrently",
        "input": """Board:
wK . bK
. . .
. . .
Commands:
click 50 50
click 150 150
click 250 50
click 250 150
wait 999
print board
wait 1
print board
""",
        "expected": """wK . bK
. . .
. . .
. . bK
. wK .
. . .
"""
    },

    # No redirect and no cooldown after arrival
    {
        "name": "moving_piece_cannot_be_redirected_mid_route",
        "input": """Board:
wR . .
Commands:
click 50 50
click 250 50
wait 500
click 50 50
click 150 50
wait 500
print board
wait 1000
print board
""",
        "expected": """wR . .
. . wR
"""
    },
    {
        "name": "moving_piece_still_goes_to_original_destination",
        "input": """Board:
wR . .
Commands:
click 50 50
click 250 50
wait 999
click 50 50
click 150 50
wait 1001
print board
""",
        "expected": """. . wR
"""
    },
    {
        "name": "piece_can_move_again_immediately_after_arrival",
        "input": """Board:
wR . .
Commands:
click 50 50
click 150 50
wait 1000
click 150 50
click 250 50
wait 1000
print board
""",
        "expected": """. . wR
"""
    },
    {
        "name": "piece_can_move_again_without_cooldown_after_arrival",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 150 150
wait 1000
click 150 150
click 250 250
wait 1000
print board
""",
        "expected": """. . .
. . .
. . wK
"""
    },
    {
        "name": "clicking_moving_piece_does_not_select_it",
        "input": """Board:
wR . .
. wN .
Commands:
click 50 50
click 250 50
wait 500
click 50 50
click 150 150
wait 1500
print board
""",
        "expected": """. . wR
. wN .
"""
    },
    {
        "name": "moving_piece_cannot_start_second_move_before_arrival_even_if_wait_almost_done",
        "input": """Board:
wR . .
Commands:
click 50 50
click 250 50
wait 1999
click 50 50
click 150 50
print board
wait 1
print board
""",
        "expected": """wR . .
. . wR
"""
    },
    {
        "name": "after_arrival_same_piece_can_reverse_direction_immediately",
        "input": """Board:
wR . .
Commands:
click 50 50
click 250 50
wait 2000
click 250 50
click 50 50
wait 2000
print board
""",
        "expected": """wR . .
"""
    },

    # Current common-route/global active movement restriction
    {
        "name": "two_different_pieces_cannot_move_while_one_is_already_moving",
        "input": """Board:
wR . .
bR . .
Commands:
click 50 50
click 250 50
wait 500
click 50 150
click 250 150
wait 2000
print board
""",
        "expected": """. . wR
bR . .
"""
    },
    {
        "name": "moving_piece_redirect_attempt_and_other_piece_are_blocked",
        "input": """Board:
wR . .
bR . .
. . .
Commands:
click 50 50
click 250 50
wait 500
click 50 50
click 50 150
click 50 150
click 250 150
wait 2000
print board
""",
        "expected": """. . wR
bR . .
. . .
"""
    },
    {
        "name": "piece_can_move_twice_immediately_after_each_arrival",
        "input": """Board:
wK . .
. . .
. . .
Commands:
click 50 50
click 150 50
wait 1000
click 150 50
click 250 50
wait 1000
click 250 50
click 250 150
wait 1000
print board
""",
        "expected": """. . .
. . wK
. . .
"""
    },
    {
        "name": "cannot_start_second_piece_while_first_piece_is_moving_same_color",
        "input": """Board:
wR . .
wN . .
. . .
Commands:
click 50 50
click 250 50
wait 500
click 50 150
click 150 250
wait 1500
print board
""",
        "expected": """. . wR
wN . .
. . .
"""
    },
    {
        "name": "cannot_start_second_piece_while_first_piece_is_moving_opposite_color",
        "input": """Board:
wR . .
. . .
bR . .
Commands:
click 50 50
click 250 50
click 50 250
click 250 250
wait 2000
print board
""",
        "expected": """. . wR
. . .
bR . .
"""
    },
    {
        "name": "cannot_start_second_piece_even_on_different_row_route",
        "input": """Board:
wR . .
. . .
wR . .
Commands:
click 50 50
click 250 50
click 50 250
click 250 250
wait 2000
print board
""",
        "expected": """. . wR
. . .
wR . .
"""
    },
    {
        "name": "cannot_start_second_piece_even_if_first_almost_arrived",
        "input": """Board:
wR . .
. . .
bR . .
Commands:
click 50 50
click 250 50
wait 1999
click 50 250
click 250 250
wait 1
print board
""",
        "expected": """. . wR
. . .
bR . .
"""
    },
    {
        "name": "second_piece_can_move_after_first_piece_arrived",
        "input": """Board:
wR . .
. . .
bR . .
Commands:
click 50 50
click 250 50
wait 2000
click 50 250
click 250 250
wait 2000
print board
""",
        "expected": """. . wR
. . .
. . bR
"""
    },
    {
        "name": "second_piece_can_move_after_first_arrived_immediately_no_extra_wait",
        "input": """Board:
wK . .
bK . .
. . .
Commands:
click 50 50
click 150 50
wait 1000
click 50 150
click 150 150
wait 1000
print board
""",
        "expected": """. wK .
. bK .
. . .
"""
    },
    {
        "name": "blocked_second_move_does_not_affect_first_active_move",
        "input": """Board:
wR . .
. . .
bR . .
Commands:
click 50 50
click 250 50
wait 500
click 50 250
click 250 250
wait 500
print board
wait 1000
print board
""",
        "expected": """wR . .
. . .
bR . .
. . wR
. . .
bR . .
"""
    },
    {
        "name": "cannot_start_knight_while_rook_is_moving",
        "input": """Board:
wR . .
bN . .
. . .
Commands:
click 50 50
click 250 50
wait 500
click 50 150
click 250 250
wait 1500
print board
""",
        "expected": """. . wR
bN . .
. . .
"""
    },
    {
        "name": "cannot_start_pawn_while_other_piece_is_moving",
        "input": """Board:
wR . .
. bP .
. . .
Commands:
click 50 50
click 250 50
wait 500
click 150 150
click 150 250
wait 1500
print board
""",
        "expected": """. . wR
. bP .
. . .
"""
    },
    {
        "name": "same_piece_cannot_be_redirected_and_other_piece_also_cannot_start",
        "input": """Board:
wR . .
. . .
bR . .
Commands:
click 50 50
click 250 50
wait 500
click 50 50
click 150 50
click 50 250
click 250 250
wait 1500
print board
""",
        "expected": """. . wR
. . .
bR . .
"""
    },
    # Iteration 8 - invalid moves and error stability
    {
        "name": "blocked_rook_path_does_not_change_board",
        "input": """Board:
wR wP .
. . .
. . bK
Commands:
click 50 50
click 250 50
wait 3000
print board
""",
        "expected": """wR wP .
. . .
. . bK
"""
    },
    {
        "name": "friendly_destination_does_not_change_board",
        "input": """Board:
wK wR .
. . .
. . bK
Commands:
click 50 50
click 150 50
wait 1000
print board
""",
        "expected": """wK wR .
. . .
. . bK
"""
    },
    {
        "name": "illegal_move_does_not_start_motion",
        "input": """Board:
wR . .
bR . .
Commands:
click 50 50
click 150 150
click 50 150
click 250 150
wait 2000
print board
""",
        "expected": """wR . .
. . bR
"""
    },
    {
        "name": "outside_click_with_selection_clears_selection",
        "input": """Board:
wR . .
. . .
. . bK
Commands:
click 50 50
click 500 500
click 250 50
wait 2000
print board
""",
        "expected": """wR . .
. . .
. . bK
"""
    },
    {
        "name": "outside_click_without_selection_is_ignored",
        "input": """Board:
wR . .
. . .
. . bK
Commands:
click 500 500
wait 1000
print board
""",
        "expected": """wR . .
. . .
. . bK
"""
    },
    {
        "name": "empty_first_click_does_not_select",
        "input": """Board:
wR . .
. . .
. . bK
Commands:
click 150 50
click 250 50
wait 2000
print board
""",
        "expected": """wR . .
. . .
. . bK
"""
    },
    {
        "name": "illegal_second_click_clears_selection",
        "input": """Board:
wR . .
. . .
. . bK
Commands:
click 50 50
click 150 150
click 250 50
wait 2000
print board
""",
        "expected": """wR . .
. . .
. . bK
"""
    },
    {
        "name": "motion_in_progress_rejects_second_move",
        "input": """Board:
wR . .
bR . .
Commands:
click 50 50
click 250 50
wait 500
click 50 150
click 250 150
wait 1500
print board
""",
        "expected": """. . wR
bR . .
"""
    },
{
    "name": "capturing_enemy_king_ends_game_and_blocks_later_moves",
    "input": """Board:
wR . bK
. . wN
. . .
Commands:
click 50 50
click 250 50
wait 2000
print board
click 250 150
click 50 250
wait 2000
print board
""",
    "expected": """. . wR
. . wN
. . .
. . wR
. . wN
. . .
"""
},
{
    "name": "king_capture_does_not_end_game_before_arrival",
    "input": """Board:
wR . bK
. . wN
. . .
Commands:
click 50 50
click 250 50
wait 1000
print board
wait 1000
print board
""",
    "expected": """wR . bK
. . wN
. . .
. . wR
. . wN
. . .
"""
},
{
    "name": "white_piece_captures_black_king_game_over",
    "input": """Board:
wQ . .
. bK .
. . .
Commands:
click 50 50
click 150 150
wait 1000
print board
click 50 50
click 250 50
wait 2000
print board
""",
    "expected": """. . .
. wQ .
. . .
. . .
. wQ .
. . .
"""
},
{
    "name": "black_piece_captures_white_king_game_over",
    "input": """Board:
bQ . .
. wK .
. . .
Commands:
click 50 50
click 150 150
wait 1000
print board
click 150 150
click 250 250
wait 1000
print board
""",
    "expected": """. . .
. bQ .
. . .
. . .
. bQ .
. . .
"""
},
{
    "name": "king_capture_rejected_by_motion_in_progress_does_not_end_game",
    "input": """Board:
wR . bK
bR . .
. . .
Commands:
click 50 150
click 250 150
wait 500
click 50 50
click 250 50
wait 1500
print board
click 50 50
click 250 50
wait 2000
print board
""",
    "expected": """wR . bK
. . bR
. . .
. . wR
. . bR
. . .
"""
},
{
    "name": "white_pawn_can_move_two_cells_from_start_row",
    "input": """Board:
. . .
. . .
. wP .
Commands:
click 150 250
click 150 50
wait 2000
print board
""",
    "expected": """. wQ .
. . .
. . .
"""
},
{
    "name": "black_pawn_can_move_two_cells_from_start_row",
    "input": """Board:
. . .
. bP .
. . .
. . .
Commands:
click 150 150
click 150 350
wait 2000
print board
""",
    "expected": """. . .
. bP .
. . .
. . .
"""
},
{
    "name": "pawn_cannot_move_two_cells_not_from_start_row",
    "input": """Board:
. . .
. wP .
. . .
. . .
Commands:
click 150 150
click 150 350
wait 2000
print board
""",
    "expected": """. . .
. wP .
. . .
. . .
"""
},
{
    "name": "pawn_two_cell_move_requires_clear_middle_cell",
    "input": """Board:
. . .
. bN .
. wP .
. . .
Commands:
click 150 250
click 150 50
wait 2000
print board
""",
    "expected": """. . .
. bN .
. wP .
. . .
"""
},
{
    "name": "pawn_two_cell_move_requires_empty_destination",
    "input": """Board:
. bN .
. . .
. wP .
. . .
Commands:
click 150 250
click 150 50
wait 2000
print board
""",
    "expected": """. bN .
. . .
. wP .
. . .
"""
},
{
    "name": "white_pawn_promotes_to_queen_on_last_row",
    "input": """Board:
. . .
. wP .
. . .
Commands:
click 150 150
click 150 50
wait 1000
print board
""",
    "expected": """. wQ .
. . .
. . .
"""
},
{
    "name": "black_pawn_promotes_to_queen_on_last_row",
    "input": """Board:
. . .
. bP .
. . .
Commands:
click 150 150
click 150 250
wait 1000
print board
""",
    "expected": """. . .
. . .
. bQ .
"""
},
{
    "name": "pawn_promotes_after_diagonal_capture_on_last_row",
    "input": """Board:
. bR .
. . wP
. . .
Commands:
click 250 150
click 150 50
wait 1000
print board
""",
    "expected": """. wQ .
. . .
. . .
"""
},
{
    "name": "jump_keeps_piece_on_same_logical_cell",
    "input": """Board:
wN . .
. . .
. . .
Commands:
jump 50 50
wait 999
print board
wait 1
print board
""",
    "expected": """wN . .
. . .
. . .
wN . .
. . .
. . .
"""
},
{
    "name": "airborne_piece_captures_arriving_enemy",
    "input": """Board:
wN . .
. . .
bR . .
Commands:
click 50 250
click 50 50
jump 50 50
wait 2000
print board
""",
    "expected": """wN . .
. . .
. . .
"""
},
{
    "name": "airborne_piece_lands_normally_if_no_enemy_arrives",
    "input": """Board:
wN . .
. . .
. . .
Commands:
jump 50 50
wait 1000
print board
""",
    "expected": """wN . .
. . .
. . .
"""
},
{
    "name": "moving_piece_cannot_jump",
    "input": """Board:
wR . .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 500
jump 50 50
wait 1500
print board
""",
    "expected": """. . wR
. . .
. . .
"""
},
{
    "name": "airborne_piece_cannot_start_move",
    "input": """Board:
wR . .
. . .
. . .
Commands:
jump 50 50
click 50 50
click 250 50
wait 1000
print board
""",
    "expected": """wR . .
. . .
. . .
"""
},
{
    "name": "piece_can_move_after_jump_lands",
    "input": """Board:
wR . .
. . .
. . .
Commands:
jump 50 50
wait 1000
click 50 50
click 250 50
wait 2000
print board
""",
    "expected": """. . wR
. . .
. . .
"""
},
{
    "name": "friendly_piece_cannot_arrive_on_airborne_friendly_piece",
    "input": """Board:
wN . .
. . .
wR . .
Commands:
click 50 250
click 50 50
jump 50 50
wait 2000
print board
""",
    "expected": """wN . .
. . .
wR . .
"""
},
{
    "name": "enemy_arriving_after_jump_lands_captures_normally",
    "input": """Board:
wN . .
. . .
bR . .
. . .
Commands:
click 50 250
click 50 50
jump 50 50
wait 1000
print board
wait 1000
print board
""",
    "expected": """wN . .
. . .
bR . .
. . .
bR . .
. . .
. . .
. . .
"""
},

]


def compile_program():
    if os.name == "nt":
        command = [
            "cl",
            "/EHsc",
            "/std:c++17",
            "/Iinclude",
            *CPP_FILES,
            f"/Fe:{EXE_PATH}",
        ]
    else:
        command = [
            "g++",
            "-std=c++17",
            "-Iinclude",
            *CPP_FILES,
            "-o",
            EXE_PATH,
        ]

    result = subprocess.run(
        command,
        cwd=ROOT_DIR,
        capture_output=True,
        text=True
    )

    if result.returncode != 0:
        print("Compilation failed.")
        print("Command:")
        print(" ".join(command))
        print("stdout:")
        print(result.stdout)
        print("stderr:")
        print(result.stderr)
        sys.exit(1)


def normalize_output(output):
    output = output.replace("\r\n", "\n")
    lines = output.split("\n")
    useful_lines = []

    for line in lines:
        stripped = line.strip()
        if stripped == "":
            continue
        useful_lines.append(stripped)

    if not useful_lines:
        return ""

    return "\n".join(useful_lines) + "\n"


def run_one_test(test):
    result = subprocess.run(
        [EXE_PATH],
        input=test["input"],
        capture_output=True,
        text=True,
        cwd=ROOT_DIR
    )

    actual = normalize_output(result.stdout)
    expected = normalize_output(test["expected"])

    if actual == expected:
        print(f"PASS: {test['name']}")
        return True

    print(f"FAIL: {test['name']}")
    print("Input:")
    print(test["input"])
    print("Expected:")
    print(repr(expected))
    print("Actual:")
    print(repr(actual))
    print("-" * 60)
    return False


def main():
    compile_program()

    passed = 0
    failed = 0

    for test in tests:
        if run_one_test(test):
            passed += 1
        else:
            failed += 1

    print()
    print(f"Passed: {passed}/{len(tests)}")
    print(f"Failed: {failed}")

    if failed != 0:
        sys.exit(1)


if __name__ == "__main__":
    main()