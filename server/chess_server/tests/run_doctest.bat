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
 /Ishared\cpp\include /Iserver\chess_server\include ^
 /Iclient\desktop_client\include /Iserver\chess_server\tests\doctest ^
 server\chess_server\tests\doctest\test_main.cpp ^
 server\chess_server\tests\doctest\test_position_piece.cpp ^
 server\chess_server\tests\doctest\test_board_parser_mapper.cpp ^
 server\chess_server\tests\doctest\test_rules.cpp ^
 server\chess_server\tests\doctest\test_engine_controller.cpp ^
 server\chess_server\tests\doctest\test_realtime_arbiter.cpp ^
 server\chess_server\tests\doctest\test_event_bus.cpp ^
 server\chess_server\tests\doctest\test_username.cpp ^
 shared\cpp\src\Core\Position.cpp shared\cpp\src\Core\Piece.cpp ^
 shared\cpp\src\Core\Board.cpp shared\cpp\src\Network\Protocol.cpp ^
 server\chess_server\src\IO\BoardParser.cpp ^
 server\chess_server\src\IO\BoardPrinter.cpp ^
 server\chess_server\src\IO\BoardMapper.cpp ^
 server\chess_server\src\Rules\PieceRules.cpp ^
 server\chess_server\src\Rules\RuleEngine.cpp ^
 server\chess_server\src\Engine\GameEngine.cpp ^
 server\chess_server\src\Control\Controller.cpp ^
 server\chess_server\src\Realtime\RealTimeArbiter.cpp ^
 server\chess_server\src\Messaging\EventBus.cpp ^
 server\chess_server\src\Messaging\GameEventSubscribers.cpp ^
 server\chess_server\src\Messaging\MessageBus.cpp ^
 server\chess_server\src\Messaging\EngineMessageHandler.cpp ^
 server\chess_server\src\Messaging\GameStateSnapshotBuilder.cpp ^
 client\desktop_client\src\Client\ClientGameState.cpp ^
 /Fe:doctest_tests.exe
if errorlevel 1 exit /b 1
doctest_tests.exe
exit /b %errorlevel%
