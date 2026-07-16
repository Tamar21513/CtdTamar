#pragma once

#include <string>
#include <vector>

class AnimationPlayer {
private:
    std::vector<std::string> framePaths;
    int currentFrameIndex;
    int framesPerSecond;
    bool loop;
    long long elapsedMs;

public:
    AnimationPlayer();

    void setFrames(const std::vector<std::string>& newFramePaths, int fps, bool shouldLoop);
    void update(long long deltaMs);
    std::string getCurrentFramePath() const;
    bool isEmpty() const;
};