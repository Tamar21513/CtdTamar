#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
CXX="${CXX:-g++}"
"$CXX" -std=c++17 \
  -Ishared/cpp/include -Iserver/game_server/include \
  -Iclient/game_client/include -Iserver/game_server/tests/doctest \
  server/game_server/tests/doctest/test_main.cpp \
  server/game_server/tests/doctest/test_position_piece.cpp \
  server/game_server/tests/doctest/test_board_parser_mapper.cpp \
  server/game_server/tests/doctest/test_rules.cpp \
  server/game_server/tests/doctest/test_engine_controller.cpp \
  server/game_server/tests/doctest/test_realtime_arbiter.cpp \
  server/game_server/tests/doctest/test_event_bus.cpp \
  server/game_server/tests/doctest/test_username.cpp \
  shared/cpp/src/Core/Position.cpp shared/cpp/src/Core/Piece.cpp \
  shared/cpp/src/Core/Board.cpp shared/cpp/src/Network/Protocol.cpp \
  server/game_server/src/IO/BoardParser.cpp \
  server/game_server/src/IO/BoardPrinter.cpp \
  server/game_server/src/IO/BoardMapper.cpp \
  server/game_server/src/Rules/PieceRules.cpp \
  server/game_server/src/Rules/RuleEngine.cpp \
  server/game_server/src/Engine/GameEngine.cpp \
  server/game_server/src/Control/Controller.cpp \
  server/game_server/src/Realtime/RealTimeArbiter.cpp \
  server/game_server/src/Messaging/EventBus.cpp \
  server/game_server/src/Messaging/GameEventSubscribers.cpp \
  server/game_server/src/Messaging/MessageBus.cpp \
  server/game_server/src/Messaging/EngineMessageHandler.cpp \
  server/game_server/src/Messaging/GameStateSnapshotBuilder.cpp \
  client/game_client/src/Client/ClientGameState.cpp \
  -o doctest_tests
./doctest_tests
