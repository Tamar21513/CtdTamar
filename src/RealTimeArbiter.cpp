#include "../include/RealTimeArbiter.hpp"
#include "../include/Config.hpp"

#include <cmath>
#include <algorithm>

using namespace std;

RealTimeArbiter::RealTimeArbiter() {
    currentTimeMs = 0;
}

long long RealTimeArbiter::calculateDurationMs(const Position& source, const Position& destination) const {
    int rowDistance = abs(destination.getRow() - source.getRow());
    int colDistance = abs(destination.getCol() - source.getCol());

    int cellDistance = max(rowDistance, colDistance);

    return cellDistance * Config::MOVE_TIME_PER_CELL_MS;
}

bool RealTimeArbiter::hasActiveMotion() const {
    return !activeMotions.empty();
}

void RealTimeArbiter::startMotion(shared_ptr<Piece> piece, const Position& source, const Position& destination) {
    Motion motion;

    motion.piece = piece;
    motion.source = source;
    motion.destination = destination;
    motion.startTimeMs = currentTimeMs;
    motion.finishTimeMs = currentTimeMs + calculateDurationMs(source, destination);

    if (piece != nullptr) {
        piece->setState(PieceState::Moving);
    }

    activeMotions.push_back(motion);
}

void RealTimeArbiter::startJump(shared_ptr<Piece> piece, const Position& cell) {
    Jump jump;

    jump.piece = piece;
    jump.cell = cell;
    jump.startTimeMs = currentTimeMs;
    jump.finishTimeMs = currentTimeMs + Config::JUMP_DURATION_MS;

    if (piece != nullptr) {
        piece->setState(PieceState::Airborne);
    }

    activeJumps.push_back(jump);
}

TimeEvents RealTimeArbiter::advanceTime(long long ms) {
    currentTimeMs += ms;

    TimeEvents events;

    vector<Motion> stillMoving;

    for (size_t i = 0; i < activeMotions.size(); i++) {
        Motion motion = activeMotions[i];

        if (currentTimeMs >= motion.finishTimeMs) {
            ArrivalEvent event;

            event.piece = motion.piece;
            event.source = motion.source;
            event.destination = motion.destination;

            events.arrivals.push_back(event);
        } else {
            stillMoving.push_back(motion);
        }
    }

    activeMotions = stillMoving;

    vector<Jump> stillJumping;

    for (size_t i = 0; i < activeJumps.size(); i++) {
        Jump jump = activeJumps[i];

        if (currentTimeMs >= jump.finishTimeMs) {
            JumpLandingEvent event;

            event.piece = jump.piece;
            event.cell = jump.cell;

            events.jumpLandings.push_back(event);
        } else {
            stillJumping.push_back(jump);
        }
    }

    activeJumps = stillJumping;

    return events;
}