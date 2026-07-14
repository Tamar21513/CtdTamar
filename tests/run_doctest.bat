@echo off
setlocal
cd /d "%~dp0\.."

if exist doctest_tests.exe del /q doctest_tests.exe
if exist doctest_tests.pdb del /q doctest_tests.pdb
if exist *.obj del /q *.obj

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: cl.exe was not found.
    echo Open "x64 Native Tools Command Prompt for VS 2022" and run this file again.
    exit /b 1
)

cl /nologo /EHsc /std:c++17 /Zi /Od /Iinclude /Itests\doctest ^
    tests\doctest\test_main.cpp ^
    tests\doctest\test_position_piece.cpp ^
    tests\doctest\test_board_parser_mapper.cpp ^
    tests\doctest\test_rules.cpp ^
    tests\doctest\test_engine_controller.cpp ^
    tests\doctest\test_realtime_arbiter.cpp ^
    src\Position.cpp src\Piece.cpp src\Board.cpp src\BoardParser.cpp ^
    src\BoardPrinter.cpp src\BoardMapper.cpp src\PieceRules.cpp ^
    src\RuleEngine.cpp src\GameEngine.cpp src\Controller.cpp ^
    src\RealTimeArbiter.cpp /Fe:doctest_tests.exe

if errorlevel 1 (
    echo.
    echo COMPILATION FAILED
    exit /b 1
)

echo.
echo ===== RUNNING DOCTEST =====
doctest_tests.exe
exit /b %errorlevel%
