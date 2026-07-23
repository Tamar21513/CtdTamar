#include "../../include/Graphics/AnimationPlayer.hpp"

// Implements AnimationPlayer.
AnimationPlayer::AnimationPlayer() {
    currentFrameIndex = 0;
    framesPerSecond = 8;
    loop = true;
    elapsedMs = 0;
}

// Implements setFrames.
void AnimationPlayer::setFrames(const std::vector<std::string>& newFramePaths, int fps, bool shouldLoop) {
    framePaths = newFramePaths;
    framesPerSecond = fps;
    loop = shouldLoop;
    currentFrameIndex = 0;
    elapsedMs = 0;
}

// Implements update.
void AnimationPlayer::update(long long deltaMs) {
    if (framePaths.empty()) {
        return;
    }

    if (framesPerSecond <= 0) {
        return;
    }

    elapsedMs += deltaMs;

    long long frameDurationMs = 1000 / framesPerSecond;

    while (elapsedMs >= frameDurationMs) {
        elapsedMs -= frameDurationMs;
        currentFrameIndex++;

        if (currentFrameIndex >= static_cast<int>(framePaths.size())) {
            if (loop) {
                currentFrameIndex = 0;
            } else {
                currentFrameIndex = static_cast<int>(framePaths.size()) - 1;
            }
        }
    }
}

// Implements getCurrentFramePath.
std::string AnimationPlayer::getCurrentFramePath() const {
    if (framePaths.empty()) {
        return "";
    }

    return framePaths[currentFrameIndex];
}

// Implements isEmpty.
bool AnimationPlayer::isEmpty() const {
    return framePaths.empty();
}
