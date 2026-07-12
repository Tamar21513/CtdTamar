#include "Piece.hpp"

Piece::Piece(string color, string type, long long cooldownMs) {
    this->color = color;
    this->type = type;
    this->cooldownMs = cooldownMs;
    this->lastMovedAt = 0;
}

string Piece::getColor() const {
    return color;
}

string Piece::getType() const {
    return type;
}

long long Piece::getCooldownMs() const {
    return cooldownMs;
}

long long Piece::getLastMovedAt() const {
    return lastMovedAt;
}

string Piece::token() const {
    return color + type;
}

bool Piece::isSameColor(const Piece& other) const {
    return color == other.color;
}

bool Piece::canMove(long long currentTimeMs) const {
    return currentTimeMs - lastMovedAt >= cooldownMs;
}

void Piece::markMoved(long long currentTimeMs) {
    lastMovedAt = currentTimeMs;
}