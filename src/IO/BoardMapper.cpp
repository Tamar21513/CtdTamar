#include "../../include/IO/BoardMapper.hpp"
#include "../../include/Core/Config.hpp"

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

    /*
     * ממירים את מיקום העכבר ממיקום בחלון
     * למיקום יחסי לתחילת תאי הלוח.
     */
    int relativeX = x - boardStartX;
    int relativeY = y - boardStartY;

    /*
     * לחיצה בשוליים שמסביב ללוח.
     */
    if (relativeX < 0 || relativeY < 0) {
        return nullopt;
    }

    int boardPixelWidth =
        board.getWidth() * cellSizeX;

    int boardPixelHeight =
        board.getHeight() * cellSizeY;

    /*
     * לחיצה אחרי הקצה הימני או התחתון
     * של תאי הלוח.
     */
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