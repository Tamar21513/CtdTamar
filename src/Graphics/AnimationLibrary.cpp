#include "../../include/Graphics/AnimationLibrary.hpp"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

// Implements extractNumberFromFileName.
static int extractNumberFromFileName(const std::string& fileName) {
    std::string digits;
    for (char ch : fileName) {
        if (std::isdigit(static_cast<unsigned char>(ch))) digits += ch;
    }
    return digits.empty() ? -1 : std::stoi(digits);
}

// Implements readInt.
static int readInt(const std::string& text, const std::string& key, int fallback) {
    std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    return std::regex_search(text, match, pattern) ? std::stoi(match[1].str()) : fallback;
}

// Implements readBool.
static bool readBool(const std::string& text, const std::string& key, bool fallback) {
    std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(text, match, pattern)) return fallback;
    return match[1].str() == "true";
}

// Implements getFramePaths.
std::vector<std::string> AnimationLibrary::getFramePaths(const std::string& pieceCode, VisualState state) const {
    std::vector<std::string> framePaths;
    std::string folderPath = buildSpritesFolderPath(pieceCode, state);
    if (!fs::exists(folderPath)) return framePaths;

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.path().extension() == ".png") framePaths.push_back(entry.path().string());
    }

    std::sort(framePaths.begin(), framePaths.end(), [](const std::string& a, const std::string& b) {
        int numberA = extractNumberFromFileName(fs::path(a).stem().string());
        int numberB = extractNumberFromFileName(fs::path(b).stem().string());
        if (numberA != -1 && numberB != -1) return numberA < numberB;
        return a < b;
    });
    return framePaths;
}

// Implements getConfig.
AnimationConfig AnimationLibrary::getConfig(const std::string& pieceCode, VisualState state) const {
    AnimationConfig config;
    std::ifstream input(buildStateFolderPath(pieceCode, state) + "\\config.json");
    if (!input) return config;

    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string text = buffer.str();
    config.frameCount = readInt(text, "frame_count", config.frameCount);
    config.framesPerSecond = readInt(text, "frames_per_sec", config.framesPerSecond);
    config.isLoop = readBool(text, "is_loop", config.isLoop);

    std::regex spritePattern("\\\"sprite_size\\\"\\s*:\\s*\\[\\s*([0-9]+)\\s*,\\s*([0-9]+)");
    std::smatch match;
    if (std::regex_search(text, match, spritePattern)) {
        config.spriteWidth = std::stoi(match[1].str());
        config.spriteHeight = std::stoi(match[2].str());
    }
    return config;
}

// Implements getFramesPerSecond.
int AnimationLibrary::getFramesPerSecond(const std::string& pieceCode, VisualState state) const {
    return getConfig(pieceCode, state).framesPerSecond;
}

// Implements isLoop.
bool AnimationLibrary::isLoop(const std::string& pieceCode, VisualState state) const {
    return getConfig(pieceCode, state).isLoop;
}

// Implements buildStateFolderPath.
std::string AnimationLibrary::buildStateFolderPath(const std::string& pieceCode, VisualState state) const {
    return "assets\\pieces\\" + pieceCode + "\\states\\" + visualStateToFolderName(state);
}

// Implements buildSpritesFolderPath.
std::string AnimationLibrary::buildSpritesFolderPath(const std::string& pieceCode, VisualState state) const {
    return buildStateFolderPath(pieceCode, state) + "\\sprites";
}
