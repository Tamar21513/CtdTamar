#pragma once

struct VisualMotion {
    int fromRow;
    int fromCol;
    int toRow;
    int toCol;

    long long elapsedMs;
    long long durationMs;

    bool active;
};

VisualMotion createVisualMotion(int fromRow, int fromCol, int toRow, int toCol);
double getMotionProgress(const VisualMotion& motion);