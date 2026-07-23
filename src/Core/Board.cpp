#include "../../include/Core/Board.hpp"

// Implements Board.
Board::Board() {
    this->height = 0;
    this->width = 0;
}

// Implements Board.
Board::Board(int height, int width) {
    this->height = height;
    this->width = width;

    cells.resize(height);

    for (int row = 0; row < height; row++) {
        cells[row].resize(width);
    }
}

// Implements getHeight.
int Board::getHeight() const {
    return height;
}

// Implements getWidth.
int Board::getWidth() const {
    return width;
}

// Implements isInside.
bool Board::isInside(const Position& position) const {
    int row = position.getRow();
    int col = position.getCol();

    if (row < 0 || col < 0) {
        return false;
    }

    if (row >= height) {
        return false;
    }

    if (col >= width) {
        return false;
    }

    return true;
}

// Implements getPieceAt.
shared_ptr<Piece> Board::getPieceAt(const Position& position) const {
    if (!isInside(position)) {
        return nullptr;
    }

    return cells[position.getRow()][position.getCol()];
}

// Implements isEmpty.
bool Board::isEmpty(const Position& position) const {
    return getPieceAt(position) == nullptr;
}

// Implements placePiece.
bool Board::placePiece(const Position& position, shared_ptr<Piece> piece) {
    if (!isInside(position)) {
        return false;
    }

    if (!isEmpty(position)) {
        return false;
    }

    cells[position.getRow()][position.getCol()] = piece;
    return true;
}

// Implements removePiece.
void Board::removePiece(const Position& position) {
    if (!isInside(position)) {
        return;
    }

    cells[position.getRow()][position.getCol()] = nullptr;
}

// Implements setPieceAt.
void Board::setPieceAt(const Position& position, shared_ptr<Piece> piece) {
    if (!isInside(position)) {
        return;
    }

    cells[position.getRow()][position.getCol()] = piece;
}

// Implements movePiece.
void Board::movePiece(const Position& source, const Position& destination) {
    if (!isInside(source) || !isInside(destination)) {
        return;
    }

    shared_ptr<Piece> movingPiece = cells[source.getRow()][source.getCol()];

    cells[destination.getRow()][destination.getCol()] = movingPiece;
    cells[source.getRow()][source.getCol()] = nullptr;
}
