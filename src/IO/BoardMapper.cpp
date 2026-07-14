#include "../../include/IO/BoardMapper.hpp"
#include "../../include/Core/Config.hpp"

optional<Position> BoardMapper::pixelToCell(int x, int y, const Board& board) {
    if (x < 0 || y < 0) {
        return nullopt;
    }

    int col = x / Config::CELL_SIZE;
    int row = y / Config::CELL_SIZE;

    Position position(row, col);

    if (!board.isInside(position)) {
        return nullopt;
    }

    return position;
}