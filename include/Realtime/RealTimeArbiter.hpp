#ifndef REAL_TIME_ARBITER_HPP
#define REAL_TIME_ARBITER_HPP

#include <memory>
#include <vector>
#include "../Core/Position.hpp"
#include "../Core/Piece.hpp"

using namespace std;

struct Motion {
    shared_ptr<Piece> piece;
    Position source;
    Position destination;
    Position currentCell;
    int rowStep = 0;
    int colStep = 0;
    bool directMove = false;
    long long stepDurationMs = 1000;
    long long nextStepTimeMs = 0;
    int order = 0;
};

struct Jump {
    shared_ptr<Piece> piece;
    Position cell;
    long long finishTimeMs = 0;
};

struct StepEvent {
    shared_ptr<Piece> piece;
    Position source;
    Position from;
    Position to;
    bool reachedDestination = false;
    long long eventTimeMs = 0;
    int order = 0;
};

struct JumpLandingEvent {
    shared_ptr<Piece> piece;
    Position cell;
    long long eventTimeMs = 0;
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

    long long getCurrentTimeMs() const;
    bool hasActiveMotion() const;

    void startMotion(
        shared_ptr<Piece> piece,
        const Position& source,
        const Position& destination,
        long long stepDurationMs = 1000,
        bool forceDirectMove = false
    );

    void startJump(
        shared_ptr<Piece> piece,
        const Position& cell,
        long long jumpDurationMs = 1000
    );

    TimeEvents advanceTime(long long ms);

    const vector<Motion>& getActiveMotions() const;
    const vector<Jump>& getActiveJumps() const;

    void updateMotionCell(shared_ptr<Piece> piece, const Position& cell);
    void finishMotion(shared_ptr<Piece> piece);
    void cancelMotion(shared_ptr<Piece> piece);
};

#endif
