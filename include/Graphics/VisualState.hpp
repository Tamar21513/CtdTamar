#pragma once

#include <string>
using namespace std;

enum class VisualState {
    Idle,
    Move,
    Jump,
    ShortRest,
    LongRest,
    Captured
};

string visualStateToFolderName(VisualState state);