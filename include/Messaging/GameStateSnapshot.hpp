#ifndef GAME_STATE_SNAPSHOT_HPP
#define GAME_STATE_SNAPSHOT_HPP

#include <string>
#include <vector>

#include "../Core/Position.hpp"

// State of one piece as sent by the server.
struct PieceSnapshot {
    int id = 0;

    std::string token;

    Position position;

    std::string state = "idle";

    bool hasMoved = false;

    long long remainingCooldownMs = 0;
    long long totalCooldownMs = 0;
};

// State of a piece currently moving between cells.
struct MotionSnapshot {
    int pieceId = 0;

    Position source;
    Position destination;
    Position currentCell;

    int rowStep = 0;
    int colStep = 0;

    bool directMove = false;

    long long stepDurationMs = 0;
    long long nextStepTimeMs = 0;

    int order = 0;
};

// State of a piece currently jumping.
struct JumpSnapshot {
    int pieceId = 0;

    Position cell;

    long long finishTimeMs = 0;
};

// One completed action shown in the move list.
struct MoveHistorySnapshot {
    long long completedAtMs = 0;

    std::string color;
    std::string pieceKind;

    Position source;
    Position destination;

    bool wasCapture = false;
    bool wasPromotion = false;
    bool wasJump = false;

    std::string notation;
};

// Complete authoritative game state supplied by the server.
struct GameStateSnapshot {
    long long serverTimeMs = 0;

    bool gameOver = false;

    int whiteScore = 0;
    int blackScore = 0;

    std::vector<PieceSnapshot> pieces;
    std::vector<MotionSnapshot> motions;
    std::vector<JumpSnapshot> jumps;

    std::vector<MoveHistorySnapshot> whiteMoveHistory;
    std::vector<MoveHistorySnapshot> blackMoveHistory;
};

#endif