#include "../../include/Graphics/AnimationLibrary.hpp"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cctype>

namespace fs = std::filesystem;

static int extractNumberFromFileName(const std::string& fileName) {
    std::string digits;

    for (char ch : fileName) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            digits += ch;
        }
    }

    if (digits.empty()) {
        return -1;
    }

    return std::stoi(digits);
}

std::vector<std::string> AnimationLibrary::getFramePaths(const std::string& pieceCode, VisualState state) const {
    std::vector<std::string> framePaths;

    std::string folderPath = buildSpritesFolderPath(pieceCode, state);

    if (!fs::exists(folderPath)) {
        std::cout << "Animation folder does not exist: " << folderPath << std::endl;
        return framePaths;
    }

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.path().extension() == ".png") {
            framePaths.push_back(entry.path().string());
        }
    }

    std::sort(framePaths.begin(), framePaths.end(), [](const std::string& a, const std::string& b) {
        fs::path pathA(a);
        fs::path pathB(b);

        int numberA = extractNumberFromFileName(pathA.stem().string());
        int numberB = extractNumberFromFileName(pathB.stem().string());

        if (numberA != -1 && numberB != -1) {
            return numberA < numberB;
        }

        return a < b;
    });

    std::cout << "Loaded " << framePaths.size() << " frames from: " << folderPath << std::endl;

    return framePaths;
}

int AnimationLibrary::getFramesPerSecond(VisualState state) const {
    if (state == VisualState::Idle) {
        return 4;
    }

    if (state == VisualState::Move) {
        return 8;
    }

    if (state == VisualState::Jump) {
        return 10;
    }

    if (state == VisualState::ShortRest) {
        return 5;
    }

    if (state == VisualState::LongRest) {
        return 4;
    }

    return 6;
}

bool AnimationLibrary::isLoop(VisualState state) const {
    if (state == VisualState::Jump) {
        return false;
    }

    if (state == VisualState::Captured) {
        return false;
    }

    return true;
}

std::string AnimationLibrary::buildSpritesFolderPath(const std::string& pieceCode, VisualState state) const {
    return "assets\\pieces\\" +
           pieceCode +
           "\\states\\" +
           visualStateToFolderName(state) +
           "\\sprites";
}