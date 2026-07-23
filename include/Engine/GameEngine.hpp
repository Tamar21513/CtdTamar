#ifndef GAME_ENGINE_HPP
#define GAME_ENGINE_HPP

#include <memory>
#include <vector>
#include "../Core/Board.hpp"
#include "../Core/Config.hpp"
#include "../Core/MoveHistory.hpp"
#include "../Core/Results.hpp"
#include "../Realtime/RealTimeArbiter.hpp"
#include "../Rules/RuleEngine.hpp"
#include "../Messaging/Message.hpp"

using namespace std;
using MovingPieceInfo = Motion;
using JumpInfo = Jump;

class GameEngine {
private:
    struct PendingCastle {
        shared_ptr<Piece> king;
        shared_ptr<Piece> rook;
        Position kingSource;
        Position kingDestination;
        Position rookSource;
        Position rookDestination;
    };

    Board board;
    RuleEngine ruleEngine;
    RealTimeArbiter realTime;
    bool gameOver;
    int whiteScore;
    int blackScore;
    vector<MoveHistoryEntry> whiteMoveHistory;
    vector<MoveHistoryEntry> blackMoveHistory;
    vector<PendingCastle> pendingCastles;

    bool isCastlingRequest(const Position& source, const Position& destination) const;
    bool canCastle(const Position& source, const Position& destination, PendingCastle& castle) const;
    bool isSquareAttacked(const Position& cell, PieceColor byColor) const;
    bool completeCastleFor(const StepEvent& event);

    shared_ptr<Piece> findPieceAtRealOrVirtualCell(
        const Position& cell,
        shared_ptr<Piece> exceptPiece
    ) const;

    Position getLogicalCellOfPiece(shared_ptr<Piece> piece) const;
    void removePieceFromBoard(shared_ptr<Piece> piece);
    void commitPieceToCell(
        shared_ptr<Piece> piece,
        const Position& source,
        const Position& destination
    );

    void applyTimeEvents(const TimeEvents& events);
    void processStepEvent(const StepEvent& event);
    void processJumpLanding(const JumpLandingEvent& event);
    void stopMovingPieceAt(
        shared_ptr<Piece> piece,
        const Position& source,
        const Position& finalCell,
        long long eventTimeMs,
        bool wasCapture
    );

    bool promotePawnIfNeeded(shared_ptr<Piece> piece, const Position& cell);
    int getPieceValue(PieceKind kind) const;
    void addCaptureScore(PieceColor attackerColor, PieceKind capturedKind);
    string buildMoveNotation(
        PieceKind kind,
        const Position& source,
        const Position& destination,
        bool wasCapture,
        bool wasPromotion
    ) const;
    void recordMove(
        shared_ptr<Piece> piece,
        PieceKind originalKind,
        const Position& source,
        const Position& destination,
        bool wasCapture,
        bool wasPromotion,
        bool wasJump,
        long long completedAtMs
    );

public:
    explicit GameEngine(Board board);

    MoveResult requestMove(const Position& source, const Position& destination);
    MoveResult requestJump(const Position& cell);
    Message handleMessage(const Message& request);
    void wait(long long ms);

    const Board& getBoard() const;
    long long getCurrentTimeMs() const;
    const vector<MovingPieceInfo>& getMovingPieces() const;
    const vector<JumpInfo>& getJumpingPieces() const;
    const RealTimeArbiter& getRealTimeArbiter() const;
    int getWhiteScore() const;
    int getBlackScore() const;
    const vector<MoveHistoryEntry>& getWhiteMoveHistory() const;
    const vector<MoveHistoryEntry>& getBlackMoveHistory() const;
    bool isGameOver() const;
};

#endif
