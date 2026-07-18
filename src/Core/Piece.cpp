#include "../../include/Core/Piece.hpp"

Piece::Piece(
    int id,
    PieceColor color,
    PieceKind kind
) {
    this->id = id;
    this->color = color;
    this->kind = kind;

    this->state = PieceState::Idle;

    this->cooldownStartedAtMs = 0;
    this->cooldownUntilMs = 0;
    this->totalCooldownMs = 0;
}

int Piece::getId() const {
    return id;
}

PieceColor Piece::getColor() const {
    return color;
}

PieceKind Piece::getKind() const {
    return kind;
}

PieceState Piece::getState() const {
    return state;
}

void Piece::setState(
    PieceState newState
) {
    state = newState;
}

void Piece::setKind(
    PieceKind newKind
) {
    kind = newKind;
}

void Piece::startCooldown(
    long long currentTimeMs,
    long long durationMs
) {
    if (durationMs < 0) {
        durationMs = 0;
    }

    cooldownStartedAtMs = currentTimeMs;
    totalCooldownMs = durationMs;

    cooldownUntilMs =
        currentTimeMs + durationMs;
}

bool Piece::isOnCooldown(
    long long currentTimeMs
) const {
    return (
        state == PieceState::Idle &&
        currentTimeMs < cooldownUntilMs
    );
}

long long Piece::getRemainingCooldownMs(
    long long currentTimeMs
) const {
    if (!isOnCooldown(currentTimeMs)) {
        return 0;
    }

    return cooldownUntilMs - currentTimeMs;
}

long long Piece::getTotalCooldownMs() const {
    return totalCooldownMs;
}

double Piece::getCooldownRatio(
    long long currentTimeMs
) const {
    if (
        totalCooldownMs <= 0 ||
        !isOnCooldown(currentTimeMs)
    ) {
        return 0.0;
    }

    long long remaining =
        getRemainingCooldownMs(
            currentTimeMs
        );

    double ratio =
        static_cast<double>(remaining) /
        static_cast<double>(totalCooldownMs);

    if (ratio < 0.0) {
        return 0.0;
    }

    if (ratio > 1.0) {
        return 1.0;
    }

    return ratio;
}

string Piece::token() const {
    string result;

    result += colorToChar(color);
    result += kindToChar(kind);

    return result;
}

PieceColor Piece::colorFromChar(
    char colorChar
) {
    if (colorChar == 'w') {
        return PieceColor::White;
    }

    return PieceColor::Black;
}

PieceKind Piece::kindFromChar(
    char kindChar
) {
    if (kindChar == 'K') {
        return PieceKind::King;
    }

    if (kindChar == 'Q') {
        return PieceKind::Queen;
    }

    if (kindChar == 'R') {
        return PieceKind::Rook;
    }

    if (kindChar == 'B') {
        return PieceKind::Bishop;
    }

    if (kindChar == 'N') {
        return PieceKind::Knight;
    }

    return PieceKind::Pawn;
}

char Piece::colorToChar(
    PieceColor color
) {
    if (color == PieceColor::White) {
        return 'w';
    }

    return 'b';
}

char Piece::kindToChar(
    PieceKind kind
) {
    if (kind == PieceKind::King) {
        return 'K';
    }

    if (kind == PieceKind::Queen) {
        return 'Q';
    }

    if (kind == PieceKind::Rook) {
        return 'R';
    }

    if (kind == PieceKind::Bishop) {
        return 'B';
    }

    if (kind == PieceKind::Knight) {
        return 'N';
    }

    return 'P';
}