#include "../../include/Core/Board.hpp"

Board::Board() {
    this->height = 0;
    this->width = 0;
}

Board::Board(int height, int width) {
    this->height = height;
    this->width = width;

    cells.resize(height);

    for (int row = 0; row < height; row++) {
        cells[row].resize(width);
    }
}

int Board::getHeight() const {
    return height;
}

int Board::getWidth() const {
    return width;
}

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

shared_ptr<Piece> Board::getPieceAt(const Position& position) const {
    if (!isInside(position)) {
        return nullptr;
    }

    return cells[position.getRow()][position.getCol()];
}

bool Board::isEmpty(const Position& position) const {
    return getPieceAt(position) == nullptr;
}

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

void Board::removePiece(const Position& position) {
    if (!isInside(position)) {
        return;
    }

    cells[position.getRow()][position.getCol()] = nullptr;
}

void Board::setPieceAt(const Position& position, shared_ptr<Piece> piece) {
    if (!isInside(position)) {
        return;
    }

    cells[position.getRow()][position.getCol()] = piece;
}

void Board::movePiece(const Position& source, const Position& destination) {
    if (!isInside(source) || !isInside(destination)) {
        return;
    }

    shared_ptr<Piece> movingPiece = cells[source.getRow()][source.getCol()];

    cells[destination.getRow()][destination.getCol()] = movingPiece;
    cells[source.getRow()][source.getCol()] = nullptr;
}