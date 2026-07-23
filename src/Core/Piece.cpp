#include "../../include/Core/Piece.hpp"

// Implements Piece.
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
    this->hasMoved = false;
}

// Implements getId.
int Piece::getId() const {
    return id;
}

// Implements getColor.
PieceColor Piece::getColor() const {
    return color;
}

// Implements getKind.
PieceKind Piece::getKind() const {
    return kind;
}

// Implements getState.
PieceState Piece::getState() const {
    return state;
}

// Implements setState.
void Piece::setState(
    PieceState newState
) {
    state = newState;
}

// Implements setKind.
void Piece::setKind(
    PieceKind newKind
) {
    kind = newKind;
}

// Implements startCooldown.
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

// Implements isOnCooldown.
bool Piece::isOnCooldown(
    long long currentTimeMs
) const {
    return (
        state == PieceState::Idle &&
        currentTimeMs < cooldownUntilMs
    );
}

// Implements getRemainingCooldownMs.
long long Piece::getRemainingCooldownMs(
    long long currentTimeMs
) const {
    if (!isOnCooldown(currentTimeMs)) {
        return 0;
    }

    return cooldownUntilMs - currentTimeMs;
}

// Implements getTotalCooldownMs.
long long Piece::getTotalCooldownMs() const {
    return totalCooldownMs;
}

// Implements getCooldownRatio.
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

// Implements token.
string Piece::token() const {
    string result;

    result += colorToChar(color);
    result += kindToChar(kind);

    return result;
}

// Implements colorFromChar.
PieceColor Piece::colorFromChar(
    char colorChar
) {
    if (colorChar == 'w') {
        return PieceColor::White;
    }

    return PieceColor::Black;
}

// Implements kindFromChar.
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

// Implements colorToChar.
char Piece::colorToChar(
    PieceColor color
) {
    if (color == PieceColor::White) {
        return 'w';
    }

    return 'b';
}

// Implements kindToChar.
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

bool Piece::getHasMoved() const {
    return hasMoved;
}

void Piece::markAsMoved() {
    hasMoved = true;
}
