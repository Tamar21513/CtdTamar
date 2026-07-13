#ifndef REAL_TIME_ARBITER_HPP
#define REAL_TIME_ARBITER_HPP

#include <vector>
#include <memory>

#include "Position.hpp"
#include "Piece.hpp"

using namespace std;

struct Motion {
    shared_ptr<Piece> piece;
    Position source;
    Position destination;
    long long startTimeMs;
    long long finishTimeMs;
};

struct Jump {
    shared_ptr<Piece> piece;
    Position cell;
    long long startTimeMs;
    long long finishTimeMs;
};

struct ArrivalEvent {
    shared_ptr<Piece> piece;
    Position source;
    Position destination;
};

struct JumpLandingEvent {
    shared_ptr<Piece> piece;
    Position cell;
};

struct TimeEvents {
    vector<ArrivalEvent> arrivals;
    vector<JumpLandingEvent> jumpLandings;
};

class RealTimeArbiter {
private:
    long long currentTimeMs;
    vector<Motion> activeMotions;
    vector<Jump> activeJumps;

    long long calculateDurationMs(const Position& source, const Position& destination) const;

public:
    RealTimeArbiter();
    bool hasActiveMotion() const;
    void startMotion(shared_ptr<Piece> piece, const Position& source, const Position& destination);
    void startJump(shared_ptr<Piece> piece, const Position& cell);
    TimeEvents advanceTime(long long ms);
};

#endif