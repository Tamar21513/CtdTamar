#include "../include/PieceRules.hpp"

#include <cmath>
using namespace std;

bool PieceRules::isPathClear(const Board& board, const Position& source, const Position& destination) {
    int fromRow = source.getRow();
    int fromCol = source.getCol();
    int toRow = destination.getRow();
    int toCol = destination.getCol();
    int rowStep = 0;
    int colStep = 0;

    if (toRow > fromRow) {
        rowStep = 1;
    } else if (toRow < fromRow) {
        rowStep = -1;
    }

    if (toCol > fromCol) {
        colStep = 1;
    } else if (toCol < fromCol) {
        colStep = -1;
    }

    int currentRow = fromRow + rowStep;
    int currentCol = fromCol + colStep;

    while (currentRow != toRow || currentCol != toCol) {
        Position currentPosition(currentRow, currentCol);

        if (!board.isEmpty(currentPosition)) {
            return false;
        }

        currentRow += rowStep;
        currentCol += colStep;
    }

    return true;
}

//צריח
bool PieceRules::canMoveLikeRook(const Board& board, const Position& source, const Position& destination) {
    if (source == destination) {
        return false;
    }

    bool sameRow = source.getRow() == destination.getRow();
    bool sameCol = source.getCol() == destination.getCol();

    if (!sameRow && !sameCol) {
        return false;
    }

    return isPathClear(board, source, destination);
}

//רץ
bool PieceRules::canMoveLikeBishop(const Board& board, const Position& source, const Position& destination) {
    if (source == destination) {
        return false;
    }

    int rowDiff = abs(destination.getRow() - source.getRow());
    int colDiff = abs(destination.getCol() - source.getCol());

    if (rowDiff != colDiff) {
        return false;
    }

    return isPathClear(board, source, destination);
}

//מלכה
bool PieceRules::canMoveLikeQueen(const Board& board, const Position& source, const Position& destination) {
    return canMoveLikeRook(board, source, destination) || canMoveLikeBishop(board, source, destination);
}

//פרש
bool PieceRules::canMoveLikeKnight(const Position& source, const Position& destination) {
    if (source == destination) {
        return false;
    }

    int rowDiff = abs(destination.getRow() - source.getRow());
    int colDiff = abs(destination.getCol() - source.getCol());

    return (rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2);
}

//מלך
bool PieceRules::canMoveLikeKing(const Position& source, const Position& destination) {
    if (source == destination) {
        return false;
    }

    int rowDiff = abs(destination.getRow() - source.getRow());
    int colDiff = abs(destination.getCol() - source.getCol());

    return rowDiff <= 1 && colDiff <= 1;
}

//רגלי
bool PieceRules::canMoveLikePawn(const Board& board, const Position& source, const Position& destination, PieceColor color) {
    if (source == destination) {
        return false;
    }

    int direction = 0;
    int startRow = 0;

    if (color == PieceColor::White) {
        direction = -1;
        startRow = board.getHeight() - 1;
    } else {
        direction = 1;
        startRow = 0;
    }

    int rowDiff = destination.getRow() - source.getRow();
    int colDiff = destination.getCol() - source.getCol();

    // צעד אחד קדימה
    if (rowDiff == direction && colDiff == 0) {
        return board.isEmpty(destination);
    }

    // שני צעדים קדימה מהשורה ההתחלתית בלבד
    if (source.getRow() == startRow && rowDiff == 2 * direction && colDiff == 0) {
        
        Position middleCell(source.getRow() + direction, source.getCol());

        return board.isEmpty(middleCell) && board.isEmpty(destination);
    }

    // אכילה באלכסון — רק כלי בצבע נגדי
    if (rowDiff == direction && abs(colDiff) == 1) {
        shared_ptr<Piece> targetPiece = board.getPieceAt(destination);

        if (targetPiece == nullptr) {
            return false;
        }

        return targetPiece->getColor() != color;
    }

    return false;
}