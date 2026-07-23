#include "../../include/Realtime/RealTimeArbiter.hpp"
#include <algorithm>

// Implements RealTimeArbiter.
RealTimeArbiter::RealTimeArbiter()
    : currentTimeMs(0), nextMotionOrder(0) {}

// Implements signValue.
int RealTimeArbiter::signValue(int value) const {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

// Implements getCurrentTimeMs.
long long RealTimeArbiter::getCurrentTimeMs() const {
    return currentTimeMs;
}

// Implements hasActiveMotion.
bool RealTimeArbiter::hasActiveMotion() const {
    return !activeMotions.empty();
}

// Implements startMotion.
void RealTimeArbiter::startMotion(
    shared_ptr<Piece> piece,
    const Position& source,
    const Position& destination,
    long long stepDurationMs,
    bool forceDirectMove
) {
    if (piece == nullptr) return;
    if (stepDurationMs <= 0) stepDurationMs = 1;

    Motion motion;
    motion.piece = piece;
    motion.source = source;
    motion.destination = destination;
    motion.currentCell = source;
    motion.rowStep = signValue(destination.getRow() - source.getRow());
    motion.colStep = signValue(destination.getCol() - source.getCol());
    motion.directMove = forceDirectMove || piece->getKind() == PieceKind::Knight;
    motion.stepDurationMs = stepDurationMs;
    motion.nextStepTimeMs = currentTimeMs + stepDurationMs;
    motion.order = nextMotionOrder++;

    piece->setState(PieceState::Moving);
    activeMotions.push_back(motion);
}

// Implements startJump.
void RealTimeArbiter::startJump(
    shared_ptr<Piece> piece,
    const Position& cell,
    long long jumpDurationMs
) {
    if (piece == nullptr) return;
    if (jumpDurationMs <= 0) jumpDurationMs = 1;

    Jump jump;
    jump.piece = piece;
    jump.cell = cell;
    jump.finishTimeMs = currentTimeMs + jumpDurationMs;
    piece->setState(PieceState::Airborne);
    activeJumps.push_back(jump);
}

// Implements advanceTime.
TimeEvents RealTimeArbiter::advanceTime(long long ms) {
    TimeEvents events;
    if (ms < 0) return events;
    currentTimeMs += ms;

    for (Motion& motion : activeMotions) {
        if (motion.piece == nullptr || motion.piece->getState() != PieceState::Moving) continue;

        while (currentTimeMs >= motion.nextStepTimeMs) {
            Position nextCell = motion.directMove
                ? motion.destination
                : Position(
                    motion.currentCell.getRow() + motion.rowStep,
                    motion.currentCell.getCol() + motion.colStep
                );

            StepEvent event;
            event.piece = motion.piece;
            event.source = motion.source;
            event.from = motion.currentCell;
            event.to = nextCell;
            event.reachedDestination = nextCell == motion.destination;
            event.eventTimeMs = motion.nextStepTimeMs;
            event.order = motion.order;
            events.steps.push_back(event);

            // GameEngine decides whether this step is accepted. Stop emitting
            // more steps in the same advance until it applies the event.
            break;
        }
    }

    sort(events.steps.begin(), events.steps.end(), [](const StepEvent& a, const StepEvent& b) {
        if (a.eventTimeMs != b.eventTimeMs) return a.eventTimeMs < b.eventTimeMs;
        return a.order < b.order;
    });

    vector<Jump> stillJumping;
    for (const Jump& jump : activeJumps) {
        if (jump.piece != nullptr && currentTimeMs >= jump.finishTimeMs) {
            JumpLandingEvent event;
            event.piece = jump.piece;
            event.cell = jump.cell;
            event.eventTimeMs = jump.finishTimeMs;
            events.jumpLandings.push_back(event);
        } else {
            stillJumping.push_back(jump);
        }
    }
    activeJumps.swap(stillJumping);

    activeMotions.erase(
        remove_if(activeMotions.begin(), activeMotions.end(),
            [](const Motion& motion) {
                return motion.piece == nullptr || motion.piece->getState() != PieceState::Moving;
            }),
        activeMotions.end()
    );

    return events;
}

// Implements getActiveMotions.
const vector<Motion>& RealTimeArbiter::getActiveMotions() const {
    return activeMotions;
}

// Implements getActiveJumps.
const vector<Jump>& RealTimeArbiter::getActiveJumps() const {
    return activeJumps;
}

// Implements updateMotionCell.
void RealTimeArbiter::updateMotionCell(shared_ptr<Piece> piece, const Position& cell) {
    for (Motion& motion : activeMotions) {
        if (motion.piece == piece) {
            motion.currentCell = cell;
            motion.nextStepTimeMs += motion.stepDurationMs;
            return;
        }
    }
}

// Implements finishMotion.
void RealTimeArbiter::finishMotion(shared_ptr<Piece> piece) {
    cancelMotion(piece);
}

// Implements cancelMotion.
void RealTimeArbiter::cancelMotion(shared_ptr<Piece> piece) {
    activeMotions.erase(
        remove_if(activeMotions.begin(), activeMotions.end(),
            [piece](const Motion& motion) { return motion.piece == piece; }),
        activeMotions.end()
    );
}
