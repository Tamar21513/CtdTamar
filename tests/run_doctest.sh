#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
"$CXX" -std=c++17 -O0 -g -Wall -Wextra -pedantic \
  -Iinclude -Itests/doctest \
  tests/doctest/test_main.cpp \
  tests/doctest/test_position_piece.cpp \
  tests/doctest/test_board_parser_mapper.cpp \
  tests/doctest/test_rules.cpp \
  tests/doctest/test_engine_controller.cpp \
  tests/doctest/test_realtime_arbiter.cpp \
  src/Position.cpp src/Piece.cpp src/Board.cpp src/BoardParser.cpp \
  src/BoardPrinter.cpp src/BoardMapper.cpp src/PieceRules.cpp \
  src/RuleEngine.cpp src/GameEngine.cpp src/Controller.cpp \
  src/RealTimeArbiter.cpp -o doctest_tests

./doctest_tests
