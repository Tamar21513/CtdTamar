# Tests and coverage

The doctest suite is organized by responsibility:

- `test_position_piece.cpp`: core position and piece behavior.
- `test_board_parser_mapper.cpp`: board storage, parsing, printing, and pixel mapping.
- `test_rules.cpp`: piece rules and move validation.
- `test_engine_controller.cpp`: timing, concurrent movement, collisions, jumps, promotion, and input control.
- `test_realtime_arbiter.cpp`: real-time event scheduling.

`test_main.cpp` defines the doctest entry point. Do not compile the application `main.cpp` into the test executable.

## Windows

Open an x64 Native Tools Command Prompt for Visual Studio 2022, change to the project directory, and run:

```bat
tests\run_doctest.bat
```

For a coverage report, install OpenCppCoverage and run:

```bat
tests\run_coverage.bat
```

## Linux or MinGW

```bash
bash tests/run_doctest.sh
```

The suite includes timing boundaries, pixel boundaries, invalid input, concurrent movement, collisions, jumps, and pawn promotion.
