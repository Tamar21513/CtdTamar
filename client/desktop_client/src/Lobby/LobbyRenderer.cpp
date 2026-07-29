#include "Lobby/LobbyRenderer.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <string>

namespace ctd::lobby {
namespace {

const cv::Scalar Background(246, 247, 250);
const cv::Scalar Surface(255, 255, 255);
const cv::Scalar Ink(37, 39, 46);
const cv::Scalar Muted(117, 119, 128);
const cv::Scalar Border(224, 226, 232);
const cv::Scalar Primary(198, 105, 42);
const cv::Scalar PrimaryHover(218, 124, 53);
const cv::Scalar Green(91, 158, 58);
const cv::Scalar Red(69, 76, 210);

void text(
    cv::Mat& frame,
    const std::string& value,
    cv::Point origin,
    double scale,
    cv::Scalar color,
    int thickness = 1) {
    cv::putText(
        frame, value, origin, cv::FONT_HERSHEY_DUPLEX,
        scale, color, thickness, cv::LINE_AA);
}

void panel(
    cv::Mat& frame,
    const cv::Rect& rectangle,
    cv::Scalar fill = Surface,
    cv::Scalar border = Border) {
    cv::rectangle(
        frame,
        {rectangle.x + 4, rectangle.y + 5,
         rectangle.width, rectangle.height},
        cv::Scalar(230, 231, 235),
        cv::FILLED,
        cv::LINE_AA);
    cv::rectangle(frame, rectangle, fill, cv::FILLED, cv::LINE_AA);
    cv::rectangle(frame, rectangle, border, 1, cv::LINE_AA);
}

void button(
    cv::Mat& frame,
    const cv::Rect& rectangle,
    const std::string& label,
    bool primary,
    bool enabled,
    bool hovered,
    bool focused = false) {
    cv::Scalar fill = primary ? Primary : Surface;
    cv::Scalar border = primary ? Primary : cv::Scalar(194, 197, 207);
    cv::Scalar color = primary ? Surface : Ink;
    if (!enabled) {
        fill = cv::Scalar(222, 224, 230);
        border = fill;
        color = cv::Scalar(155, 157, 164);
    } else if (hovered) {
        fill = primary ? PrimaryHover : cv::Scalar(247, 242, 238);
    }
    cv::rectangle(frame, rectangle, fill, cv::FILLED, cv::LINE_AA);
    cv::rectangle(
        frame, rectangle,
        focused ? cv::Scalar(235, 150, 76) : border,
        focused ? 3 : 1,
        cv::LINE_AA);
    int baseline = 0;
    const auto size = cv::getTextSize(
        label, cv::FONT_HERSHEY_DUPLEX, 0.55, 1, &baseline);
    text(
        frame,
        label,
        {rectangle.x + (rectangle.width - size.width) / 2,
         rectangle.y + (rectangle.height + size.height) / 2},
        0.55,
        color,
        1);
}

std::string clipped(
    const std::string& value,
    std::size_t maximum = 26) {
    return value.size() <= maximum
        ? value
        : value.substr(0, maximum - 3) + "...";
}

bool hover(const cv::Rect& rectangle, const cv::Point& mouse) {
    return mouse.x >= 0 && rectangle.contains(mouse);
}

void inputField(
    cv::Mat& frame,
    const cv::Rect& rectangle,
    const std::string& value,
    bool focused,
    const std::string& placeholder) {
    cv::rectangle(frame, rectangle, Surface, cv::FILLED, cv::LINE_AA);
    cv::rectangle(
        frame,
        rectangle,
        focused ? Primary : Border,
        focused ? 2 : 1,
        cv::LINE_AA);
    text(
        frame,
        value.empty() ? placeholder : clipped(value, 42),
        {rectangle.x + 16, rectangle.y + 35},
        0.58,
        value.empty() ? Muted : Ink,
        1);
}

}  // namespace

cv::Mat LobbyRenderer::render(
    const LobbyViewModel& view,
    const LobbyLayoutResult& layout,
    const cv::Point& mousePosition) const {
    cv::Mat frame(
        layout.windowSize.height,
        layout.windowSize.width,
        CV_8UC3,
        Background);
    if (view.screen == LobbyScreen::Authentication ||
        view.screen == LobbyScreen::Connecting) {
        drawAuthentication(frame, view, layout, mousePosition);
    } else if (view.screen == LobbyScreen::Lobby) {
        drawLobby(frame, view, layout, mousePosition);
    } else {
        drawPlaceholder(frame, view, layout, mousePosition);
    }
    if (view.modal != LobbyModal::None) {
        drawModal(frame, view, layout, mousePosition);
    }
    return frame;
}

void LobbyRenderer::drawAuthentication(
    cv::Mat& frame,
    const LobbyViewModel& view,
    const LobbyLayoutResult& layout,
    const cv::Point& mouse) const {
    panel(frame, layout.authenticationPanel);
    const auto& box = layout.authenticationPanel;
    text(
        frame, "KUNG FU CHESS",
        {box.x + 50, box.y + 62},
        1.0, Ink, 2);
    text(
        frame, "Sign in to enter the live lobby",
        {box.x + 50, box.y + 94},
        0.48, Muted, 1);
    text(
        frame, "USERNAME",
        {layout.usernameField.x, layout.usernameField.y - 10},
        0.42, Muted, 1);
    inputField(
        frame,
        layout.usernameField,
        view.usernameInput,
        view.focusedField == AuthenticationField::Username,
        "player_name");
    text(
        frame, "PASSWORD",
        {layout.passwordField.x, layout.passwordField.y - 10},
        0.42, Muted, 1);
    inputField(
        frame,
        layout.passwordField,
        std::string(view.passwordLength, '*'),
        view.focusedField == AuthenticationField::Password,
        "at least 10 characters");
    const bool enabled =
        view.pendingAction == PendingLobbyAction::None;
    button(
        frame, layout.loginButton,
        view.pendingAction == PendingLobbyAction::Login
            ? "SIGNING IN..." : "LOGIN",
        true, enabled,
        hover(layout.loginButton, mouse));
    button(
        frame, layout.registerButton,
        view.pendingAction == PendingLobbyAction::Register
            ? "CREATING..." : "REGISTER",
        false, enabled,
        hover(layout.registerButton, mouse));
    if (!view.visibleError.empty()) {
        text(
            frame,
            clipped(view.visibleError, 62),
            {box.x + 50, box.y + box.height - 34},
            0.45, Red, 1);
    } else if (!view.statusMessage.empty()) {
        text(
            frame,
            view.statusMessage,
            {box.x + 50, box.y + box.height - 34},
            0.45, Primary, 1);
    } else {
        text(
            frame, "SERVER  127.0.0.1:8000",
            {box.x + 50, box.y + box.height - 34},
            0.42, Muted, 1);
    }
}

void LobbyRenderer::drawHeader(
    cv::Mat& frame,
    const LobbyViewModel& view,
    const LobbyLayoutResult& layout,
    const cv::Point& mouse) const {
    cv::rectangle(frame, layout.header, Surface, cv::FILLED);
    cv::line(
        frame,
        {0, layout.header.height - 1},
        {layout.header.width, layout.header.height - 1},
        Border, 1, cv::LINE_AA);
    text(frame, "KUNG FU CHESS", {36, 42}, 0.82, Ink, 2);
    text(
        frame,
        "LOBBY  /  " + clipped(view.authenticatedUsername, 18),
        {36, 68}, 0.42, Muted, 1);
    const bool enabled = view.networkActionsEnabled();
    button(
        frame, layout.createRoomButton, "CREATE ROOM",
        true, enabled, hover(layout.createRoomButton, mouse));
    button(
        frame, layout.joinHiddenButton, "JOIN WITH CODE",
        false, enabled, hover(layout.joinHiddenButton, mouse));
    button(
        frame, layout.logoutButton, "LOGOUT",
        false, view.pendingAction == PendingLobbyAction::None,
        hover(layout.logoutButton, mouse));
    const auto status =
        connectionStatusText(view.connectionState);
    const cv::Scalar statusColor =
        view.connectionState ==
                ctd::network::WebSocketConnectionState::Connected
            ? Green : Red;
    text(
        frame, status,
        {layout.createRoomButton.x - 126, 53},
        0.38, statusColor, 1);
}

void LobbyRenderer::drawRoomCard(
    cv::Mat& frame,
    const ctd::network::LobbyRoom& room,
    const cv::Rect& rectangle,
    bool active,
    bool enabled,
    bool pending,
    const cv::Point& mouse) const {
    panel(frame, rectangle);
    const std::string badge = active ? "LIVE" : "WAITING";
    const cv::Scalar badgeColor = active ? Red : Primary;
    text(
        frame, badge,
        {rectangle.x + 20, rectangle.y + 28},
        0.42, badgeColor, 1);
    if (active) {
        text(
            frame,
            "WHITE   " +
                clipped(room.white ? room.white->username : "-", 18),
            {rectangle.x + 20, rectangle.y + 57},
            0.47, Ink, 1);
        text(
            frame,
            "BLACK   " +
                clipped(room.black ? room.black->username : "-", 18),
            {rectangle.x + 20, rectangle.y + 82},
            0.47, Ink, 1);
        text(
            frame,
            std::to_string(room.spectatorCount) + " watching",
            {rectangle.x + 20, rectangle.y + rectangle.height - 22},
            0.4, Muted, 1);
    } else {
        text(
            frame,
            clipped(room.host ? room.host->username : "Unknown host", 24),
            {rectangle.x + 20, rectangle.y + 66},
            0.62, Ink, 1);
        text(
            frame, "Ready for an opponent",
            {rectangle.x + 20, rectangle.y + 91},
            0.4, Muted, 1);
    }
    const auto action = LobbyLayout::cardActionButton(rectangle);
    button(
        frame, action,
        pending ? "WAIT..." : (active ? "WATCH" : "JOIN"),
        active ? false : true,
        enabled && !pending,
        hover(action, mouse));
}

void LobbyRenderer::drawLobby(
    cv::Mat& frame,
    const LobbyViewModel& view,
    const LobbyLayoutResult& layout,
    const cv::Point& mouse) const {
    drawHeader(frame, view, layout, mouse);
    text(
        frame, "WAITING FOR OPPONENT",
        {layout.waitingSection.x, layout.waitingSection.y + 30},
        0.62, Ink, 1);
    text(
        frame, "Public rooms you can join",
        {layout.waitingSection.x, layout.waitingSection.y + 52},
        0.4, Muted, 1);
    text(
        frame, "LIVE ROOMS",
        {layout.activeSection.x, layout.activeSection.y + 30},
        0.62, Ink, 1);
    text(
        frame, "Watch games currently in progress",
        {layout.activeSection.x, layout.activeSection.y + 52},
        0.4, Muted, 1);

    if (view.waitingRooms.empty()) {
        text(
            frame, "No public rooms are waiting.",
            {layout.waitingSection.x + 12,
             layout.waitingSection.y + 112},
            0.55, Muted, 1);
    }
    if (view.activeRooms.empty()) {
        text(
            frame, "No live rooms are available.",
            {layout.activeSection.x + 12,
             layout.activeSection.y + 112},
            0.55, Muted, 1);
    }
    const int waitingStart =
        view.waitingPage * layout.cardsPerPage;
    for (std::size_t index = 0;
         index < layout.waitingCards.size();
         ++index) {
        const int roomIndex =
            waitingStart + static_cast<int>(index);
        if (roomIndex >= static_cast<int>(view.waitingRooms.size())) {
            break;
        }
        const auto& room = view.waitingRooms[roomIndex];
        drawRoomCard(
            frame, room, layout.waitingCards[index], false,
            view.networkActionsEnabled(),
            view.pendingAction ==
                    PendingLobbyAction::JoinPublicRoom &&
                view.pendingRoomId == room.roomId,
            mouse);
    }
    const int activeStart =
        view.activePage * layout.cardsPerPage;
    for (std::size_t index = 0;
         index < layout.activeCards.size();
         ++index) {
        const int roomIndex =
            activeStart + static_cast<int>(index);
        if (roomIndex >= static_cast<int>(view.activeRooms.size())) {
            break;
        }
        const auto& room = view.activeRooms[roomIndex];
        drawRoomCard(
            frame, room, layout.activeCards[index], true,
            view.networkActionsEnabled(),
            view.pendingAction == PendingLobbyAction::WatchRoom &&
                view.pendingRoomId == room.roomId,
            mouse);
    }
    if (!view.visibleError.empty()) {
        const cv::Rect warning{
            layout.content.x,
            layout.header.height + 4,
            layout.content.width,
            34};
        cv::rectangle(frame, warning, cv::Scalar(238, 232, 255), cv::FILLED);
        text(
            frame, clipped(view.visibleError, 110),
            {warning.x + 12, warning.y + 23},
            0.43, Red, 1);
    }
}

void LobbyRenderer::drawModal(
    cv::Mat& frame,
    const LobbyViewModel& view,
    const LobbyLayoutResult& layout,
    const cv::Point& mouse) const {
    cv::Mat dim = frame.clone();
    cv::rectangle(
        dim, {0, 0, frame.cols, frame.rows},
        cv::Scalar(30, 31, 36), cv::FILLED);
    cv::addWeighted(dim, 0.35, frame, 0.65, 0, frame);
    panel(frame, layout.modal);
    if (view.modal == LobbyModal::CreateRoom) {
        text(
            frame, "CREATE A ROOM",
            {layout.modal.x + 60, layout.modal.y + 60},
            0.78, Ink, 2);
        text(
            frame,
            "Public rooms are visible. Hidden rooms use an invite code.",
            {layout.modal.x + 60, layout.modal.y + 98},
            0.42, Muted, 1);
        button(
            frame, layout.modalPrimaryButton, "PUBLIC",
            true, view.pendingAction == PendingLobbyAction::None,
            hover(layout.modalPrimaryButton, mouse));
        button(
            frame, layout.modalSecondaryButton, "HIDDEN",
            false, view.pendingAction == PendingLobbyAction::None,
            hover(layout.modalSecondaryButton, mouse));
    } else {
        text(
            frame, "JOIN HIDDEN ROOM",
            {layout.modal.x + 60, layout.modal.y + 60},
            0.78, Ink, 2);
        text(
            frame, "Enter the six-character invitation code.",
            {layout.modal.x + 60, layout.modal.y + 92},
            0.42, Muted, 1);
        inputField(
            frame, layout.hiddenCodeField,
            view.hiddenRoomCodeInput, true, "ABC234");
        button(
            frame, layout.modalPrimaryButton, "JOIN",
            true, view.pendingAction == PendingLobbyAction::None,
            hover(layout.modalPrimaryButton, mouse));
        button(
            frame, layout.modalSecondaryButton, "CANCEL",
            false, view.pendingAction == PendingLobbyAction::None,
            hover(layout.modalSecondaryButton, mouse));
    }
    if (!view.visibleError.empty()) {
        text(
            frame,
            clipped(view.visibleError, 64),
            {layout.modal.x + 60, layout.modal.y + 205},
            0.4, Red, 1);
    }
}

void LobbyRenderer::drawPlaceholder(
    cv::Mat& frame,
    const LobbyViewModel& view,
    const LobbyLayoutResult& layout,
    const cv::Point& mouse) const {
    cv::rectangle(frame, layout.header, Surface, cv::FILLED);
    button(
        frame, layout.backButton, "BACK TO LOBBY",
        false, true, hover(layout.backButton, mouse));
    const cv::Rect center{
        layout.windowSize.width / 2 - 340,
        layout.windowSize.height / 2 - 190,
        680,
        380};
    panel(frame, center);
    if (view.screen == LobbyScreen::SpectatorPlaceholder &&
        view.spectator) {
        text(frame, "LIVE SPECTATOR", {center.x + 60, center.y + 68},
             0.9, Red, 2);
        text(
            frame,
            "WHITE   " + view.spectator->whiteUsername,
            {center.x + 60, center.y + 130},
            0.65, Ink, 1);
        text(
            frame,
            "BLACK   " + view.spectator->blackUsername,
            {center.x + 60, center.y + 172},
            0.65, Ink, 1);
        text(
            frame,
            std::to_string(view.spectator->spectatorCount) +
                " spectators",
            {center.x + 60, center.y + 220},
            0.48, Muted, 1);
        text(
            frame,
            "Board streaming is not connected yet.",
            {center.x + 60, center.y + 292},
            0.52, Primary, 1);
    } else if (view.screen == LobbyScreen::RoomReady &&
               view.roomReady) {
        text(frame, "ROOM READY", {center.x + 60, center.y + 82},
             1.0, Green, 2);
        text(
            frame,
            "You are " + view.roomReady->assignedColor,
            {center.x + 60, center.y + 150},
            0.68, Ink, 1);
        text(
            frame,
            "Opponent: " + view.roomReady->opponentUsername,
            {center.x + 60, center.y + 195},
            0.62, Ink, 1);
        text(
            frame,
            "Game Server allocation is a later stage.",
            {center.x + 60, center.y + 275},
            0.5, Primary, 1);
    } else {
        const bool hidden =
            view.screen == LobbyScreen::HiddenRoomWaiting;
        text(
            frame,
            hidden ? "HIDDEN ROOM CREATED" : "WAITING FOR OPPONENT",
            {center.x + 60, center.y + 86},
            0.85, Ink, 2);
        if (hidden) {
            text(
                frame, "INVITE CODE",
                {center.x + 60, center.y + 145},
                0.45, Muted, 1);
            text(
                frame, view.hiddenRoomCode,
                {center.x + 60, center.y + 210},
                1.4, Primary, 3);
            text(
                frame,
                "This code is private and is not listed publicly.",
                {center.x + 60, center.y + 270},
                0.46, Muted, 1);
        } else {
            text(
                frame,
                "The room will start when another player joins.",
                {center.x + 60, center.y + 175},
                0.5, Muted, 1);
        }
    }
}

}  // namespace ctd::lobby
