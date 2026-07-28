#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
CXX="${CXX:-g++}"
"$CXX" -std=c++17 \
  -Ishared/cpp/include -Iserver/chess_server/include \
  -Iclient/desktop_client/include -Iserver/chess_server/tests/doctest \
  server/chess_server/tests/doctest/test_main.cpp \
  server/chess_server/tests/doctest/test_position_piece.cpp \
  server/chess_server/tests/doctest/test_board_parser_mapper.cpp \
  server/chess_server/tests/doctest/test_rules.cpp \
  server/chess_server/tests/doctest/test_engine_controller.cpp \
  server/chess_server/tests/doctest/test_realtime_arbiter.cpp \
  server/chess_server/tests/doctest/test_event_bus.cpp \
  server/chess_server/tests/doctest/test_username.cpp \
  shared/cpp/src/Core/Position.cpp shared/cpp/src/Core/Piece.cpp \
  shared/cpp/src/Core/Board.cpp shared/cpp/src/Network/Protocol.cpp \
  server/chess_server/src/IO/BoardParser.cpp \
  server/chess_server/src/IO/BoardPrinter.cpp \
  server/chess_server/src/IO/BoardMapper.cpp \
  server/chess_server/src/Rules/PieceRules.cpp \
  server/chess_server/src/Rules/RuleEngine.cpp \
  server/chess_server/src/Engine/GameEngine.cpp \
  server/chess_server/src/Control/Controller.cpp \
  server/chess_server/src/Realtime/RealTimeArbiter.cpp \
  server/chess_server/src/Messaging/EventBus.cpp \
  server/chess_server/src/Messaging/GameEventSubscribers.cpp \
  server/chess_server/src/Messaging/MessageBus.cpp \
  server/chess_server/src/Messaging/EngineMessageHandler.cpp \
  server/chess_server/src/Messaging/GameStateSnapshotBuilder.cpp \
  client/desktop_client/src/Client/ClientGameState.cpp \
  -o doctest_tests
./doctest_tests
