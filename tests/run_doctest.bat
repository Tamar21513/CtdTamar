@echo off
setlocal
cd /d "%~dp0\.."
if exist doctest_tests.exe del /q doctest_tests.exe
where cl >nul 2>nul
if errorlevel 1 (
  echo ERROR: cl.exe was not found. Open x64 Native Tools Command Prompt for VS 2022.
  exit /b 1
)
cl /nologo /EHsc /std:c++17 /Zi /Od ^
 /Iinclude /Iinclude\Core /Iinclude\IO /Iinclude\Rules /Iinclude\Engine ^
 /Iinclude\Control /Iinclude\Realtime /Itests\doctest ^
 tests\doctest\test_main.cpp tests\doctest\test_position_piece.cpp ^
 tests\doctest\test_board_parser_mapper.cpp tests\doctest\test_rules.cpp ^
 tests\doctest\test_engine_controller.cpp tests\doctest\test_realtime_arbiter.cpp ^
 src\Core\Position.cpp src\Core\Piece.cpp src\Core\Board.cpp ^
 src\IO\BoardParser.cpp src\IO\BoardPrinter.cpp src\IO\BoardMapper.cpp ^
 src\Rules\PieceRules.cpp src\Rules\RuleEngine.cpp ^
 src\Engine\GameEngine.cpp src\Control\Controller.cpp ^
 src\Realtime\RealTimeArbiter.cpp /Fe:doctest_tests.exe
if errorlevel 1 exit /b 1
doctest_tests.exe
exit /b %errorlevel%
