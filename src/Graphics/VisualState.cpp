#include "../../include/Graphics/VisualState.hpp"

// Implements visualStateToFolderName.
string visualStateToFolderName(VisualState state) {
    if (state == VisualState::Idle) {
        return "idle";
    }

    if (state == VisualState::Move) {
        return "move";
    }

    if (state == VisualState::Jump) {
        return "jump";
    }

    if (state == VisualState::ShortRest) {
        return "short_rest";
    }

    if (state == VisualState::LongRest) {
        return "long_rest";
    }

    if (state == VisualState::Captured) {
        return "captured";
    }

    return "idle";
}
