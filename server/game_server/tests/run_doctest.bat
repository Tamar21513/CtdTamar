@echo off
setlocal
cd /d "%~dp0\..\..\.."
if exist doctest_tests.exe del /q doctest_tests.exe
where cl >nul 2>nul
if errorlevel 1 (
  echo ERROR: cl.exe was not found. Open x64 Native Tools Command Prompt for VS 2022.
  exit /b 1
)
cl /nologo /EHsc /std:c++17 /Zi /Od ^
 /Ishared\cpp\include /Iserver\game_server\include ^
 /Iclient\game_client\include /Iserver\game_server\tests\doctest ^
 server\game_server\tests\doctest\test_main.cpp ^
 server\game_server\tests\doctest\test_position_piece.cpp ^
 server\game_server\tests\doctest\test_board_parser_mapper.cpp ^
 server\game_server\tests\doctest\test_rules.cpp ^
 server\game_server\tests\doctest\test_engine_controller.cpp ^
 server\game_server\tests\doctest\test_realtime_arbiter.cpp ^
 server\game_server\tests\doctest\test_event_bus.cpp ^
 server\game_server\tests\doctest\test_username.cpp ^
 shared\cpp\src\Core\Position.cpp shared\cpp\src\Core\Piece.cpp ^
 shared\cpp\src\Core\Board.cpp shared\cpp\src\Network\Protocol.cpp ^
 server\game_server\src\IO\BoardParser.cpp ^
 server\game_server\src\IO\BoardPrinter.cpp ^
 server\game_server\src\IO\BoardMapper.cpp ^
 server\game_server\src\Rules\PieceRules.cpp ^
 server\game_server\src\Rules\RuleEngine.cpp ^
 server\game_server\src\Engine\GameEngine.cpp ^
 server\game_server\src\Control\Controller.cpp ^
 server\game_server\src\Realtime\RealTimeArbiter.cpp ^
 server\game_server\src\Messaging\EventBus.cpp ^
 server\game_server\src\Messaging\GameEventSubscribers.cpp ^
 server\game_server\src\Messaging\MessageBus.cpp ^
 server\game_server\src\Messaging\EngineMessageHandler.cpp ^
 server\game_server\src\Messaging\GameStateSnapshotBuilder.cpp ^
 client\game_client\src\Client\ClientGameState.cpp ^
 /Fe:doctest_tests.exe
if errorlevel 1 exit /b 1
doctest_tests.exe
exit /b %errorlevel%
