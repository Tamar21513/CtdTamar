#pragma once

#include <opencv2/core/types.hpp>

#include <vector>

namespace ctd::lobby {

struct LobbyLayoutResult {
    cv::Size windowSize;
    cv::Rect header;
    cv::Rect content;
    cv::Rect waitingSection;
    cv::Rect activeSection;
    cv::Rect authenticationPanel;
    cv::Rect usernameField;
    cv::Rect passwordField;
    cv::Rect loginButton;
    cv::Rect registerButton;
    cv::Rect createRoomButton;
    cv::Rect joinHiddenButton;
    cv::Rect logoutButton;
    cv::Rect modal;
    cv::Rect modalPrimaryButton;
    cv::Rect modalSecondaryButton;
    cv::Rect hiddenCodeField;
    cv::Rect backButton;
    std::vector<cv::Rect> waitingCards;
    std::vector<cv::Rect> activeCards;
    int columns = 1;
    int cardsPerPage = 1;
};

class LobbyLayout {
public:
    static constexpr int MinimumWidth = 1200;
    static constexpr int MinimumHeight = 700;

    LobbyLayoutResult calculate(
        int width,
        int height,
        int waitingCardCount,
        int activeCardCount,
        int waitingPage = 0,
        int activePage = 0) const;

    int columnsForWidth(int width) const;
    static cv::Rect cardActionButton(const cv::Rect& card);
};

}  // namespace ctd::lobby
