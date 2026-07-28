#ifndef GAME_EVENT_HPP
#define GAME_EVENT_HPP

#include "Core/Piece.hpp"
#include "Core/Position.hpp"
#include <string>

enum class GameEventType {
    MoveCompleted,
    ScoreUpdated,
    GameStarted,
    GameEnded,
    SoundRequested
};

struct GameEvent {
    GameEventType type = GameEventType::MoveCompleted;
    Position source;
    Position destination;
    PieceKind movingPieceKind = PieceKind::Pawn;
    PieceColor movingPieceColor = PieceColor::White;
    bool wasCapture = false;
    bool hasCapturedPiece = false;
    PieceKind capturedPieceKind = PieceKind::Pawn;
    PieceColor capturedPieceColor = PieceColor::Black;
    bool wasPromotion = false;
    bool wasJump = false;
    long long completedAtMs = 0;
    int whiteScore = 0;
    int blackScore = 0;
    bool hasWinner = false;
    PieceColor winner = PieceColor::White;
    std::string gameEndReason;
    std::string sound;
};

#endif
