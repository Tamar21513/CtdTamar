#ifndef CONFIG_HPP
#define CONFIG_HPP

namespace Config {
    const int CELL_SIZE = 100;
    const long long DEFAULT_COOLDOWN_MS = 6000;
    const long long MOVE_TIME_PER_CELL_MS = 1000;

    const char BOARD_HEADER[] = "Board:";
    const char COMMANDS_HEADER[] = "Commands:";
    const char CLICK_COMMAND[] = "click";
    const char WAIT_COMMAND[] = "wait";
    const char PRINT_COMMAND[] = "print";
    const char BOARD_WORD[] = "board";
    const char EMPTY_CELL[] = ".";

    const char ERROR_UNKNOWN_TOKEN[] = "ERROR UNKNOWN_TOKEN";
    const char ERROR_ROW_WIDTH_MISMATCH[] = "ERROR ROW_WIDTH_MISMATCH";
}

#endif
