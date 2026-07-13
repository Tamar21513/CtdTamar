#ifndef GAME_ENGINE_HPP
#define GAME_ENGINE_HPP

#include "Board.hpp"
#include "Position.hpp"
#include "RuleEngine.hpp"
#include "RealTimeArbiter.hpp"
#include "Results.hpp"

class GameEngine {
private:
    Board board;
    RuleEngine ruleEngine;
    RealTimeArbiter realTimeArbiter;
    bool gameOver;

    void applyArrival(const ArrivalEvent& event);
    void applyJumpLanding(const JumpLandingEvent& event);

public:
    GameEngine(Board board);
    MoveResult requestMove(const Position& source, const Position& destination);
    MoveResult requestJump(const Position& cell);
    void wait(long long ms);
    const Board& getBoard() const;
    bool isGameOver() const;
};

#endif