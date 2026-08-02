#ifndef PLAYER_DISPLAY_HPP
#define PLAYER_DISPLAY_HPP

#include <string>

struct PlayerScoreDisplay {
    std::string topName;
    int topScore = 0;
    std::string bottomName;
    int bottomScore = 0;
};

inline PlayerScoreDisplay buildPlayerScoreDisplay(
    const std::string& whiteUsername,
    const std::string& blackUsername,
    int whiteScore,
    int blackScore,
    int whiteRating = -1,
    int blackRating = -1
) {
    PlayerScoreDisplay display;
    display.topName = blackUsername.empty()
        ? "Waiting for opponent"
        : (blackRating >= 0
            ? blackUsername + " (" + std::to_string(blackRating) + ")"
            : blackUsername);
    display.topScore = blackScore;
    display.bottomName = whiteUsername.empty()
        ? "Waiting for opponent"
        : (whiteRating >= 0
            ? whiteUsername + " (" + std::to_string(whiteRating) + ")"
            : whiteUsername);
    display.bottomScore = whiteScore;
    return display;
}

#endif
