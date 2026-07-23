#include "../../include/Core/Position.hpp"

// Implements Position.
Position::Position() {
    this->row = -1;
    this->col = -1;
}

// Implements Position.
Position::Position(int row, int col) {
    this->row = row;
    this->col = col;
}

// Implements getRow.
int Position::getRow() const {
    return row;
}

// Implements getCol.
int Position::getCol() const {
    return col;
}

// Implements function.
bool Position::operator==(const Position& other) const {
    return row == other.row && col == other.col;
}

// Implements function.
bool Position::operator!=(const Position& other) const {
    return !(*this == other);
}

// Implements toString.
string Position::toString() const {
    return "(" + to_string(row) + "," + to_string(col) + ")";
}
