#include "../../include/Graphics/VisualStateMachine.hpp"

// Implements chooseState.
VisualState VisualStateMachine::chooseState(PieceState pieceState, long long remainingCooldownMs, long long totalCooldownMs) const {
    if (pieceState == PieceState::Captured) {
        return VisualState::Captured;
    }

    if (pieceState == PieceState::Moving) {
        return VisualState::Move;
    }

    if (pieceState == PieceState::Airborne) {
        return VisualState::Jump;
    }

    if (remainingCooldownMs > 0) {
        if (totalCooldownMs >= 5000) {
            return VisualState::LongRest;
        }

        return VisualState::ShortRest;
    }

    return VisualState::Idle;
}
