#pragma once

#include <string>
#include <vector>

#include "VisualState.hpp"

class AnimationLibrary {
public:
    std::vector<std::string> getFramePaths(const std::string& pieceCode, VisualState state) const;
    int getFramesPerSecond(VisualState state) const;
    bool isLoop(VisualState state) const;

private:
    std::string buildSpritesFolderPath(const std::string& pieceCode, VisualState state) const;
};