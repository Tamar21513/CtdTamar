#ifndef RESULTS_HPP
#define RESULTS_HPP

#include <string>
using namespace std;

struct MoveValidation {
    bool isValid;
    string reason;
};

struct MoveResult {
    bool isAccepted;
    string reason;
};

struct ControllerResult {
    bool handled;
    string reason;
};

namespace Reasons {
    const string OK = "ok";
    const string OUTSIDE_BOARD = "outside_board";
    const string EMPTY_SOURCE = "empty_source";
    const string FRIENDLY_DESTINATION = "friendly_destination";
    const string ILLEGAL_PIECE_MOVE = "illegal_piece_move";
    const string GAME_OVER = "game_over";
    const string NO_SELECTION = "no_selection";
    const string SELECTED = "selected";
    const string CLICK_OUTSIDE = "click_outside";
    const string EMPTY_CLICK = "empty_click";
    const string MOTION_IN_PROGRESS = "motion_in_progress";
}

#endif