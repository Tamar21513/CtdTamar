#ifndef GAME_ENGINE_HPP
#define GAME_ENGINE_HPP

#include <vector>
#include <memory>

#include "Board.hpp"
#include "Position.hpp"
#include "RuleEngine.hpp"
#include "Results.hpp"
#include "Config.hpp"

using namespace std;

struct MovingPieceInfo {
    shared_ptr<Piece> piece;
    Position source;
    Position destination;
    Position currentVirtualCell;
    int rowStep;
    int colStep;
    long long nextStepTimeMs;
    int order;
};

struct JumpInfo {
    shared_ptr<Piece> piece;
    Position cell;
    long long finishTimeMs;
};

class GameEngine {
private:
    Board board;
    RuleEngine ruleEngine;
    bool gameOver;

    long long currentTimeMs;
    int nextMoveOrder;

    vector<MovingPieceInfo> movingPieces;
    vector<JumpInfo> jumpingPieces;

    int signValue(int value) const;
    void processTimeEvents();
    int findNextMovingPieceIndex() const;
    int findNextJumpIndex() const;
    void processMovingPieceStep(int movingIndex);
    void processJumpLanding(int jumpIndex);

    shared_ptr<Piece> findPieceAtRealOrVirtualCell(const Position& cell, shared_ptr<Piece> exceptPiece) const;
    int findMovingPieceIndex(shared_ptr<Piece> piece) const;
    void removeMovingPiece(shared_ptr<Piece> piece);

    Position getLogicalCellOfPiece(shared_ptr<Piece> piece) const;
    void removePieceFromBoard(shared_ptr<Piece> piece);
    void commitPieceToCell(shared_ptr<Piece> piece, const Position& source, const Position& destination);
    void stopMovingPieceAt(MovingPieceInfo& movingPiece, const Position& finalCell);
    void promotePawnIfNeeded(shared_ptr<Piece> piece, const Position& cell);

public:
    GameEngine(Board board);

    MoveResult requestMove(const Position& source, const Position& destination);
    MoveResult requestJump(const Position& cell);

    void wait(long long ms);

    const Board& getBoard() const;
    bool isGameOver() const;
};

#endif