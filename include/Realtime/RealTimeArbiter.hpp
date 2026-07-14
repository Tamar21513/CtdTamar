#ifndef REAL_TIME_ARBITER_HPP
#define REAL_TIME_ARBITER_HPP

#include <vector>
#include <memory>

#include "../Core/Position.hpp"
#include "../Core/Piece.hpp"

using namespace std;

struct Motion {
    shared_ptr<Piece> piece;
    Position source;
    Position destination;
    Position currentCell;
    int rowStep;
    int colStep;
    long long nextStepTimeMs;
    bool finished;
    int order;
};

struct Jump {
    shared_ptr<Piece> piece;
    Position cell;
    long long startTimeMs;
    long long finishTimeMs;
};

struct StepEvent {
    shared_ptr<Piece> piece;
    Position source;
    Position from;
    Position to;
    bool reachedDestination;
    long long eventTimeMs;
    int order;
};

struct JumpLandingEvent {
    shared_ptr<Piece> piece;
    Position cell;
};

struct TimeEvents {
    vector<StepEvent> steps;
    vector<JumpLandingEvent> jumpLandings;
};

class RealTimeArbiter {
private:
    long long currentTimeMs;
    vector<Motion> activeMotions;
    vector<Jump> activeJumps;
    int nextMotionOrder;
    int signValue(int value) const;

public:
    RealTimeArbiter();
    bool hasActiveMotion() const;
    void startMotion(shared_ptr<Piece> piece, const Position& source, const Position& destination);
    void startJump(shared_ptr<Piece> piece, const Position& cell);
    TimeEvents advanceTime(long long ms);
};

#endif