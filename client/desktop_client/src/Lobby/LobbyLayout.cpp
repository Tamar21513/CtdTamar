#include "Lobby/LobbyLayout.hpp"

#include <algorithm>

namespace ctd::lobby {
namespace {

std::vector<cv::Rect> cardRects(
    const cv::Rect& section,
    int columns,
    int count,
    int page,
    int cardsPerPage) {
    constexpr int gap = 18;
    constexpr int titleHeight = 54;
    const int visible = std::min(count, cardsPerPage);
    const int cardWidth =
        (section.width - gap * (columns - 1)) / columns;
    const int rows = std::max(1, cardsPerPage / columns);
    const int availableHeight = section.height - titleHeight;
    const int cardHeight =
        std::max(120, (availableHeight - gap * (rows - 1)) / rows);
    std::vector<cv::Rect> result;
    result.reserve(visible);
    const int start = page * cardsPerPage;
    const int end = std::min(count, start + cardsPerPage);
    for (int index = start; index < end; ++index) {
        const int local = index - start;
        const int row = local / columns;
        const int column = local % columns;
        result.emplace_back(
            section.x + column * (cardWidth + gap),
            section.y + titleHeight + row * (cardHeight + gap),
            cardWidth,
            cardHeight);
    }
    return result;
}

}  // namespace

int LobbyLayout::columnsForWidth(int width) const {
    if (width >= 1600) {
        return 4;
    }
    if (width >= MinimumWidth) {
        return 3;
    }
    return 2;
}

cv::Rect LobbyLayout::cardActionButton(const cv::Rect& card) {
    return {
        card.x + card.width - 116,
        card.y + card.height - 46,
        96,
        32};
}

LobbyLayoutResult LobbyLayout::calculate(
    int width,
    int height,
    int waitingCardCount,
    int activeCardCount,
    int waitingPage,
    int activePage,
    bool registrationMode) const {
    width = std::max(width, MinimumWidth);
    height = std::max(height, MinimumHeight);
    constexpr int margin = 36;
    constexpr int headerHeight = 92;
    constexpr int sectionGap = 24;
    LobbyLayoutResult result;
    result.windowSize = {width, height};
    result.header = {0, 0, width, headerHeight};
    result.content = {
        margin,
        headerHeight + 22,
        width - margin * 2,
        height - headerHeight - margin - 22};
    const int sectionHeight =
        (result.content.height - sectionGap) / 2;
    result.waitingSection = {
        result.content.x,
        result.content.y,
        result.content.width,
        sectionHeight};
    result.activeSection = {
        result.content.x,
        result.content.y + sectionHeight + sectionGap,
        result.content.width,
        result.content.height - sectionHeight - sectionGap};
    result.columns = columnsForWidth(width);
    result.cardsPerPage = result.columns;
    result.waitingCards = cardRects(
        result.waitingSection,
        result.columns,
        waitingCardCount,
        waitingPage,
        result.cardsPerPage);
    result.activeCards = cardRects(
        result.activeSection,
        result.columns,
        activeCardCount,
        activePage,
        result.cardsPerPage);

    result.playButton = {width - 670, 25, 110, 44};
    result.createRoomButton = {width - 548, 25, 150, 44};
    result.joinHiddenButton = {width - 386, 25, 176, 44};
    result.logoutButton = {width - 196, 25, 124, 44};

    const int panelWidth = 520;
    const int panelHeight = registrationMode ? 520 : 440;
    result.authenticationPanel = {
        (width - panelWidth) / 2,
        (height - panelHeight) / 2,
        panelWidth,
        panelHeight};
    result.usernameField = {
        result.authenticationPanel.x + 50,
        result.authenticationPanel.y + 130,
        panelWidth - 100,
        54};
    result.passwordField = {
        result.authenticationPanel.x + 50,
        result.authenticationPanel.y + 216,
        panelWidth - 100,
        54};
    result.confirmPasswordField = registrationMode
        ? cv::Rect{
            result.authenticationPanel.x + 50,
            result.authenticationPanel.y + 302,
            panelWidth - 100,
            54}
        : cv::Rect{};
    const int buttonY = registrationMode
        ? result.authenticationPanel.y + 390
        : result.authenticationPanel.y + 304;
    result.loginButton = {
        result.authenticationPanel.x + 50,
        buttonY,
        196,
        52};
    result.registerButton = {
        result.authenticationPanel.x + 274,
        buttonY,
        196,
        52};
    result.authModeToggle = {
        result.authenticationPanel.x + 50,
        result.authenticationPanel.y + panelHeight - 74,
        panelWidth - 100,
        28};

    result.modal = {
        (width - 560) / 2,
        (height - 440) / 2,
        560,
        440};
    result.roomNameField = {
        result.modal.x + 60,
        result.modal.y + 122,
        result.modal.width - 120,
        54};
    result.publicVisibilityButton = {
        result.modal.x + 60,
        result.modal.y + 218,
        200,
        48};
    result.hiddenVisibilityButton = {
        result.modal.x + 300,
        result.modal.y + 218,
        200,
        48};
    result.hiddenCodeField = {
        result.modal.x + 60,
        result.modal.y + 120,
        result.modal.width - 120,
        54};
    result.modalPrimaryButton = {
        result.modal.x + 60,
        result.modal.y + 326,
        200,
        52};
    result.modalSecondaryButton = {
        result.modal.x + 300,
        result.modal.y + 326,
        200,
        52};
    result.backButton = {margin, 25, 160, 44};
    return result;
}

}  // namespace ctd::lobby
