#pragma once

#include <string>
#include "Piece.hpp"
#include "Position.hpp"

struct MoveHistoryEntry {
    long long completedAtMs = 0;
    PieceColor color = PieceColor::White;
    PieceKind pieceKind = PieceKind::Pawn;
    Position source;
    Position destination;
    bool wasCapture = false;
    bool wasPromotion = false;
    bool wasJump = false;
    std::string notation;
};
