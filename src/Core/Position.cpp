#include "../../include/Core/Position.hpp"

Position::Position() {
    this->row = -1;
    this->col = -1;
}

Position::Position(int row, int col) {
    this->row = row;
    this->col = col;
}

int Position::getRow() const {
    return row;
}

int Position::getCol() const {
    return col;
}

bool Position::operator==(const Position& other) const {
    return row == other.row && col == other.col;
}

bool Position::operator!=(const Position& other) const {
    return !(*this == other);
}

string Position::toString() const {
    return "(" + to_string(row) + "," + to_string(col) + ")";
}