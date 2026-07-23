#include "../../include/IO/BoardMapper.hpp"
#include "../../include/Core/Config.hpp"

// Maps a pixel using the default board geometry.
optional<Position> BoardMapper::pixelToCell(
    int x,
    int y,
    const Board& board
) {
    return pixelToCell(
        x,
        y,
        board,
        0,
        0,
        Config::CELL_SIZE,
        Config::CELL_SIZE
    );
}

// Maps a pixel using explicit board geometry.
optional<Position> BoardMapper::pixelToCell(
    int x,
    int y,
    const Board& board,
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY
) {
    if (cellSizeX <= 0 || cellSizeY <= 0) {
        return nullopt;
    }

    // Convert the pointer to coordinates relative to the board cells.
    int relativeX = x - boardStartX;
    int relativeY = y - boardStartY;

    // Reject clicks in the margins before the board.
    if (relativeX < 0 || relativeY < 0) {
        return nullopt;
    }

    int boardPixelWidth =
        board.getWidth() * cellSizeX;

    int boardPixelHeight =
        board.getHeight() * cellSizeY;

    // Reject clicks beyond the last row or column.
    if (
        relativeX >= boardPixelWidth ||
        relativeY >= boardPixelHeight
    ) {
        return nullopt;
    }

    int col = relativeX / cellSizeX;
    int row = relativeY / cellSizeY;

    Position position(row, col);

    if (!board.isInside(position)) {
        return nullopt;
    }

    return position;
}
