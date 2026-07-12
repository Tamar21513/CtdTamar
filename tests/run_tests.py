import subprocess
import os
import sys
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent
EXECUTABLE = PROJECT_DIR / "main.exe"

COMPILE_COMMAND = [
    "cl",
    "/EHsc",
    "/std:c++17",
    "/Iinclude",
    "main.cpp",
    str(Path("src") / "Piece.cpp"),
    str(Path("src") / "Rules.cpp"),
    str(Path("src") / "BoardUtils.cpp"),
    str(Path("src") / "Game.cpp"),
    "/Fe:" + str(EXECUTABLE)
]

tests = [
    {
        "name": "parse_rectangular_board",
        "input": """Board:
wK . .
. . .
. . bK
Commands:
print board
""",
        "expected": """wK . .
. . .
. . bK
"""
    },
    {
        "name": "reject_unknown_token",
        "input": """Board:
wK xZ
. .
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
. bK
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
click 250 250
wait 1000
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
click 350 50
click -10 50
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
click 50 250
wait 1000
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
        "name": "friendly_piece_replaces_selection",
        "input": """Board:
wR . wK
. . .
Commands:
click 50 50
click 250 50
click 250 150
wait 1000
print board
""",
        "expected": """wR . .
. . wK
"""
    },
    {
        "name": "rook_horizontal_legal",
        "input": """Board:
wR . .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 1000
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
wait 1000
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
wait 1000
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
wait 1000
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
wait 1000
print board
""",
        "expected": """. . wR
. . .
. . .
"""
    },
    {
        "name": "bishop_diagonal_legal",
        "input": """Board:
wB . .
. . .
. . .
Commands:
click 50 50
click 250 250
wait 1000
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
click 50 250
wait 1000
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
. wN .
. . .
Commands:
click 50 50
click 250 250
wait 1000
print board
""",
        "expected": """wB . .
. wN .
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
wait 1000
print board
""",
        "expected": """. . .
. . .
. . wB
"""
    },
    {
        "name": "queen_horizontal_legal",
        "input": """Board:
wQ . .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 1000
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
wait 1000
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
wait 1000
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
wQ wN .
. . .
. . .
Commands:
click 50 50
click 250 50
wait 1000
print board
""",
        "expected": """wQ wN .
. . .
. . .
"""
    },
    {
        "name": "queen_blocked_diagonal",
        "input": """Board:
wQ . .
. wN .
. . .
Commands:
click 50 50
click 250 250
wait 1000
print board
""",
        "expected": """wQ . .
. wN .
. . .
"""
    },
    {
        "name": "knight_legal_jump",
        "input": """Board:
wN wP .
. . .
. . .
Commands:
click 50 50
click 150 250
wait 1000
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
wait 1000
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
wait 1000
print board
""",
        "expected": """. . .
. . .
. wN .
"""
    },
    {
        "name": "white_pawn_single_step",
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
        "expected": """. wP .
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
Commands:
click 150 150
click 150 250
wait 1000
print board
""",
        "expected": """. . .
. . .
. bP .
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
        "name": "pawn_cannot_move_two_cells",
        "input": """Board:
. . .
. . .
. wP .
. . .
Commands:
click 150 250
click 150 50
wait 1000
print board
""",
        "expected": """. . .
. . .
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
bR . .
. wP .
. . .
Commands:
click 150 150
click 50 50
wait 1000
print board
""",
        "expected": """wP . .
. . .
. . .
"""
    },
    {
        "name": "white_pawn_diagonal_capture_right",
        "input": """Board:
. . bR
. wP .
. . .
Commands:
click 150 150
click 250 50
wait 1000
print board
""",
        "expected": """. . wP
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
Commands:
click 150 150
click 50 250
wait 1000
print board
""",
        "expected": """. . .
. . .
bP . .
"""
    },
    {
        "name": "black_pawn_diagonal_capture_right",
        "input": """Board:
. . .
. bP .
. . wR
Commands:
click 150 150
click 250 250
wait 1000
print board
""",
        "expected": """. . .
. . .
. . bP
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
""",
        "expected": """wR . .
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
wait 1500
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
. wP .
. . .
Commands:
click 150 150
click 150 50
wait 999
print board
wait 1
print board
""",
        "expected": """. . .
. wP .
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
    {
        "name": "multiple_moves_arrive_independently",
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
. . .
. wK bK
. . .
"""
    },
]

def compile_project():
    if EXECUTABLE.exists():
        try:
            EXECUTABLE.unlink()
        except PermissionError:
            print("ERROR: main.exe is open/running. Close it and run tests again.")
            sys.exit(1)

    result = subprocess.run(
        COMPILE_COMMAND,
        cwd=PROJECT_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        shell=False
    )

    if result.returncode != 0:
        print("Compilation failed.")
        print("Command:")
        print(" ".join(COMPILE_COMMAND))
        print("stdout:")
        print(result.stdout)
        print("stderr:")
        print(result.stderr)
        sys.exit(1)


def normalize_output(text):
    return text.replace("\r\n", "\n")


def run_single_test(test):
    result = subprocess.run(
        [str(EXECUTABLE)],
        input=test["input"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        shell=False
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
    if result.stderr:
        print("stderr:")
        print(result.stderr)
    print("-" * 60)
    return False


def main():
    compile_project()

    passed = 0

    for test in tests:
        if run_single_test(test):
            passed += 1

    total = len(tests)
    failed = total - passed

    print()
    print(f"Passed: {passed}/{total}")
    print(f"Failed: {failed}/{total}")

    if failed != 0:
        sys.exit(1)


if __name__ == "__main__":
    main()