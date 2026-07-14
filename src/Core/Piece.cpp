#include "../../include/Core/Piece.hpp"

Piece::Piece(int id, PieceColor color, PieceKind kind) {
    this->id = id;
    this->color = color;
    this->kind = kind;
    this->state = PieceState::Idle;
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

void Piece::setState(PieceState newState) {
    state = newState;
}

void Piece::setKind(PieceKind newKind) {
    kind = newKind;
}

string Piece::token() const {
    string result = "";
    result += colorToChar(color);
    result += kindToChar(kind);
    return result;
}

PieceColor Piece::colorFromChar(char colorChar) {
    if (colorChar == 'w') {
        return PieceColor::White;
    }

    return PieceColor::Black;
}

PieceKind Piece::kindFromChar(char kindChar) {
    if (kindChar == 'K') return PieceKind::King;
    if (kindChar == 'Q') return PieceKind::Queen;
    if (kindChar == 'R') return PieceKind::Rook;
    if (kindChar == 'B') return PieceKind::Bishop;
    if (kindChar == 'N') return PieceKind::Knight;

    return PieceKind::Pawn;
}

char Piece::colorToChar(PieceColor color) {
    if (color == PieceColor::White) {
        return 'w';
    }

    return 'b';
}

char Piece::kindToChar(PieceKind kind) {
    if (kind == PieceKind::King) return 'K';
    if (kind == PieceKind::Queen) return 'Q';
    if (kind == PieceKind::Rook) return 'R';
    if (kind == PieceKind::Bishop) return 'B';
    if (kind == PieceKind::Knight) return 'N';

    return 'P';
}