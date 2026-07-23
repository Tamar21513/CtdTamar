#pragma once

#include <string>
#include <vector>
#include "VisualState.hpp"

struct AnimationConfig {
    int frameCount = 1;
    int framesPerSecond = 6;
    bool isLoop = true;
    int spriteWidth = 320;
    int spriteHeight = 320;
};

class AnimationLibrary {
public:
    std::vector<std::string> getFramePaths(const std::string& pieceCode, VisualState state) const;
    AnimationConfig getConfig(const std::string& pieceCode, VisualState state) const;
    int getFramesPerSecond(const std::string& pieceCode, VisualState state) const;
    bool isLoop(const std::string& pieceCode, VisualState state) const;

private:
    std::string buildStateFolderPath(const std::string& pieceCode, VisualState state) const;
    std::string buildSpritesFolderPath(const std::string& pieceCode, VisualState state) const;
};
