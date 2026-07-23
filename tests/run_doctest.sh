#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CXX="${CXX:-g++}"
"$CXX" -std=c++17 -O0 -g -Wall -Wextra -pedantic \
  -Iinclude -Iinclude/Core -Iinclude/IO -Iinclude/Rules -Iinclude/Engine \
  -Iinclude/Control -Iinclude/Realtime -Itests/doctest \
  tests/doctest/test_main.cpp tests/doctest/test_position_piece.cpp \
  tests/doctest/test_board_parser_mapper.cpp tests/doctest/test_rules.cpp \
  tests/doctest/test_engine_controller.cpp tests/doctest/test_realtime_arbiter.cpp \
  src/Core/Position.cpp src/Core/Piece.cpp src/Core/Board.cpp \
  src/IO/BoardParser.cpp src/IO/BoardPrinter.cpp src/IO/BoardMapper.cpp \
  src/Rules/PieceRules.cpp src/Rules/RuleEngine.cpp \
  src/Engine/GameEngine.cpp src/Control/Controller.cpp \
  src/Realtime/RealTimeArbiter.cpp -o doctest_tests
./doctest_tests
