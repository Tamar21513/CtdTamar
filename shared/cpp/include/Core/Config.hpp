#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
using namespace std;

namespace Config {
    const int CELL_SIZE = 100;
    const long long MOVE_TIME_PER_CELL_MS = 1000;
    const long long JUMP_DURATION_MS = 1000;
    const double JUMP_HEIGHT_PIXELS = 70.0;
    const string EMPTY_CELL = ".";
    const string BOARD_HEADER = "Board:";
    const string COMMANDS_HEADER = "Commands:";
    const string ERROR_UNKNOWN_TOKEN = "ERROR UNKNOWN_TOKEN";
    const string ERROR_ROW_WIDTH_MISMATCH = "ERROR ROW_WIDTH_MISMATCH";
    const long long SHORT_COOLDOWN_MS = 3000;
    const long long LONG_COOLDOWN_MS = 6000;
}
#endif