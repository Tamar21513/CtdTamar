#ifndef BOARD_MAPPER_HPP
#define BOARD_MAPPER_HPP

#include <optional>

#include "Board.hpp"
#include "Position.hpp"

using namespace std;

class BoardMapper {
public:
    static optional<Position> pixelToCell(int x, int y, const Board& board);
};

#endif