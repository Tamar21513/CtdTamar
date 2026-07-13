#ifndef BOARD_PRINTER_HPP
#define BOARD_PRINTER_HPP

#include <string>

#include "Board.hpp"

using namespace std;

class BoardPrinter {
public:
    static string print(const Board& board);
};

#endif