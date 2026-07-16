#pragma once

#include "../Core/Piece.hpp"
#include "VisualState.hpp"

class VisualStateMachine {
public:
    VisualState chooseState(PieceState pieceState, long long remainingCooldownMs, long long totalCooldownMs) const;
};