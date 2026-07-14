@echo off
setlocal
cd /d "%~dp0\.."

set "COVERAGE_EXE=C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
if not exist "%COVERAGE_EXE%" (
    echo ERROR: OpenCppCoverage was not found at:
    echo %COVERAGE_EXE%
    echo Install OpenCppCoverage or edit COVERAGE_EXE in this file.
    exit /b 1
)

call tests\run_doctest.bat
if errorlevel 1 exit /b 1

if exist coverage-report rmdir /s /q coverage-report

"%COVERAGE_EXE%" ^
  --sources "%CD%\src" ^
  --sources "%CD%\include" ^
  --excluded_sources "*doctest.h" ^
  --export_type html:"%CD%\coverage-report" ^
  -- doctest_tests.exe

if errorlevel 1 exit /b 1
start "" "%CD%\coverage-report\index.html"
