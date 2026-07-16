#include "../../include/Graphics/VisualMotion.hpp"
#include <algorithm>
#include <cstdlib>

const long long VISUAL_MOVE_TIME_PER_CELL_MS = 1000;

VisualMotion createVisualMotion(int fromRow, int fromCol, int toRow, int toCol) {
    VisualMotion motion;

    motion.fromRow = fromRow;
    motion.fromCol = fromCol;
    motion.toRow = toRow;
    motion.toCol = toCol;

    motion.elapsedMs = 0;

    int rowDistance = std::abs(toRow - fromRow);
    int colDistance = std::abs(toCol - fromCol);
    int cellDistance = std::max(rowDistance, colDistance);

    motion.durationMs = cellDistance * VISUAL_MOVE_TIME_PER_CELL_MS;

    if (motion.durationMs <= 0) {
        motion.durationMs = VISUAL_MOVE_TIME_PER_CELL_MS;
    }

    motion.active = true;

    return motion;
}

double getMotionProgress(const VisualMotion& motion) {
    if (!motion.active) {
        return 1.0;
    }

    if (motion.durationMs <= 0) {
        return 1.0;
    }

    double progress = static_cast<double>(motion.elapsedMs) / static_cast<double>(motion.durationMs);

    if (progress < 0.0) {
        return 0.0;
    }

    if (progress > 1.0) {
        return 1.0;
    }

    return progress;
}