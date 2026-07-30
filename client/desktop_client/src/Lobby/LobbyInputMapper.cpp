#include "Lobby/LobbyInputMapper.hpp"

#include <algorithm>

namespace ctd::lobby {
namespace {

bool contains(const cv::Rect& rectangle, const cv::Point& point) {
    return rectangle.contains(point);
}

}  // namespace

LobbyAction LobbyInputMapper::mapClick(
    const cv::Point& point,
    const LobbyViewModel& view,
    const LobbyLayoutResult& layout) const {
    if (view.screen == LobbyScreen::Authentication) {
        if (contains(layout.usernameField, point)) {
            return {LobbyActionType::FocusUsername, {}};
        }
        if (contains(layout.passwordField, point)) {
            return {LobbyActionType::FocusPassword, {}};
        }
        if (contains(layout.loginButton, point)) {
            return {LobbyActionType::Login, {}};
        }
        if (contains(layout.registerButton, point)) {
            return {LobbyActionType::Register, {}};
        }
        return {};
    }

    if (view.modal == LobbyModal::CreateRoom) {
        if (contains(layout.roomNameField, point)) {
            return {};
        }
        if (contains(layout.publicVisibilityButton, point)) {
            return {LobbyActionType::SelectPublicVisibility, {}};
        }
        if (contains(layout.hiddenVisibilityButton, point)) {
            return {LobbyActionType::SelectHiddenVisibility, {}};
        }
        if (contains(layout.modalPrimaryButton, point)) {
            return {LobbyActionType::SubmitCreateRoom, {}};
        }
        if (contains(layout.modalSecondaryButton, point)) {
            return {LobbyActionType::CancelModal, {}};
        }
        return {};
    }
    if (view.modal == LobbyModal::JoinHiddenRoom) {
        if (contains(layout.modalPrimaryButton, point)) {
            return {LobbyActionType::SubmitHiddenCode, {}};
        }
        if (contains(layout.modalSecondaryButton, point)) {
            return {LobbyActionType::CancelModal, {}};
        }
        return {};
    }

    if (view.screen == LobbyScreen::Lobby) {
        if (contains(layout.createRoomButton, point)) {
            return {LobbyActionType::OpenCreateRoom, {}};
        }
        if (contains(layout.joinHiddenButton, point)) {
            return {LobbyActionType::OpenJoinHidden, {}};
        }
        if (contains(layout.logoutButton, point)) {
            return {LobbyActionType::Logout, {}};
        }
        const int waitingStart =
            view.waitingPage * layout.cardsPerPage;
        for (std::size_t index = 0;
             index < layout.waitingCards.size();
             ++index) {
            const int roomIndex =
                waitingStart + static_cast<int>(index);
            if (roomIndex < static_cast<int>(view.waitingRooms.size()) &&
                contains(
                    LobbyLayout::cardActionButton(
                        layout.waitingCards[index]),
                    point)) {
                return {
                    LobbyActionType::JoinPublicRoom,
                    view.waitingRooms[roomIndex].roomId};
            }
        }
        const int activeStart =
            view.activePage * layout.cardsPerPage;
        for (std::size_t index = 0;
             index < layout.activeCards.size();
             ++index) {
            const int roomIndex =
                activeStart + static_cast<int>(index);
            if (roomIndex < static_cast<int>(view.activeRooms.size()) &&
                contains(
                    LobbyLayout::cardActionButton(
                        layout.activeCards[index]),
                    point)) {
                return {
                    LobbyActionType::WatchRoom,
                    view.activeRooms[roomIndex].roomId};
            }
        }
        return {};
    }

    if (contains(layout.backButton, point)) {
        return {LobbyActionType::BackToLobby, {}};
    }
    return {};
}

}  // namespace ctd::lobby
