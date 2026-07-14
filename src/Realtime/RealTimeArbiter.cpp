#include "../../include/Realtime/RealTimeArbiter.hpp"
#include "../../include/Core/Config.hpp"

#include <cmath>
#include <algorithm>

using namespace std;

RealTimeArbiter::RealTimeArbiter() {
    currentTimeMs = 0;
    nextMotionOrder = 0;
}

int RealTimeArbiter::signValue(int value) const {
    if (value > 0) {
        return 1;
    }

    if (value < 0) {
        return -1;
    }

    return 0;
}

bool RealTimeArbiter::hasActiveMotion() const {
    return !activeMotions.empty();
}

void RealTimeArbiter::startMotion(shared_ptr<Piece> piece, const Position& source, const Position& destination) {
    Motion motion;

    motion.piece = piece;
    motion.source = source;
    motion.destination = destination;
    motion.currentCell = source;
    motion.rowStep = signValue(destination.getRow() - source.getRow());
    motion.colStep = signValue(destination.getCol() - source.getCol());
    motion.nextStepTimeMs = currentTimeMs + Config::MOVE_TIME_PER_CELL_MS;
    motion.finished = false;
    motion.order = nextMotionOrder;

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

        if (motion.piece == nullptr || motion.piece->getState() != PieceState::Moving) {
            continue;
        }

        while (!motion.finished && currentTimeMs >= motion.nextStepTimeMs) {
            Position nextCell(motion.currentCell.getRow() + motion.rowStep, motion.currentCell.getCol() + motion.colStep);
            StepEvent event;
            event.piece = motion.piece;
            event.source = motion.source;
            event.from = motion.currentCell;
            event.to = nextCell;
            event.reachedDestination = (nextCell == motion.destination);
            event.eventTimeMs = motion.nextStepTimeMs;
            event.order = motion.order;

            events.steps.push_back(event);

            motion.currentCell = nextCell;
            motion.nextStepTimeMs += Config::MOVE_TIME_PER_CELL_MS;

            if (event.reachedDestination) {
                motion.finished = true;
            }
        }

        if (!motion.finished && motion.piece->getState() == PieceState::Moving) {
            stillMoving.push_back(motion);
        }
    }

    activeMotions = stillMoving;

    sort(events.steps.begin(), events.steps.end(), [](const StepEvent& a, const StepEvent& b) {
        if (a.eventTimeMs != b.eventTimeMs) {
            return a.eventTimeMs < b.eventTimeMs;
        }

        return a.order < b.order;
    });

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