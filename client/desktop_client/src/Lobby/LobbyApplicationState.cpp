#include "Lobby/LobbyApplicationState.hpp"

#include <algorithm>
#include <type_traits>

namespace ctd::lobby {

bool LobbyViewModel::networkActionsEnabled() const {
    return screen == LobbyScreen::Lobby &&
        connectionState ==
            ctd::network::WebSocketConnectionState::Connected &&
        pendingAction == PendingLobbyAction::None;
}

const LobbyViewModel& LobbyApplicationState::view() const {
    return view_;
}

LobbyViewModel LobbyApplicationState::snapshot() const {
    return view_;
}

void LobbyApplicationState::beginAuthentication(
    PendingLobbyAction action) {
    view_.pendingAction = action;
    view_.visibleError.clear();
    view_.statusMessage =
        action == PendingLobbyAction::Register
            ? "Creating account..."
            : "Signing in...";
}

void LobbyApplicationState::authenticationFailed(
    const std::string& message) {
    view_.screen = LobbyScreen::Authentication;
    view_.pendingAction = PendingLobbyAction::None;
    view_.statusMessage.clear();
    view_.visibleError = message;
    view_.passwordLength = 0;
}

void LobbyApplicationState::authenticationSucceeded(
    const std::string& username) {
    view_.screen = LobbyScreen::Connecting;
    view_.authenticatedUsername = username;
    view_.usernameInput = username;
    view_.passwordLength = 0;
    view_.visibleError.clear();
    view_.statusMessage = "Connecting to lobby...";
    view_.pendingAction = PendingLobbyAction::None;
}

void LobbyApplicationState::setConnectionState(
    ctd::network::WebSocketConnectionState state) {
    view_.connectionState = state;
    if (state == ctd::network::WebSocketConnectionState::Failed ||
        state == ctd::network::WebSocketConnectionState::Disconnected) {
        if (view_.screen != LobbyScreen::Authentication) {
            view_.visibleError =
                "Lobby connection is unavailable.";
        }
        view_.pendingAction = PendingLobbyAction::None;
    }
}

void LobbyApplicationState::applyEvent(
    const ctd::network::LobbyEvent& event) {
    std::visit(
        [this](const auto& value) {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (
                std::is_same_v<Event, ctd::network::ConnectedEvent>) {
                view_.authenticatedUsername = value.user.username;
                view_.statusMessage = "Loading lobby...";
            } else if constexpr (
                std::is_same_v<Event, ctd::network::LobbySnapshotEvent>) {
                view_.waitingRooms = value.waitingRooms;
                view_.activeRooms = value.activeGames;
                if (view_.screen == LobbyScreen::Connecting ||
                    view_.screen == LobbyScreen::Authentication ||
                    view_.screen == LobbyScreen::Lobby) {
                    view_.screen = LobbyScreen::Lobby;
                }
                view_.statusMessage.clear();
                view_.visibleError.clear();
                if (view_.pendingAction !=
                        PendingLobbyAction::JoinPublicRoom &&
                    view_.pendingAction !=
                        PendingLobbyAction::JoinHiddenRoom &&
                    view_.pendingAction !=
                        PendingLobbyAction::WatchRoom) {
                    view_.pendingAction = PendingLobbyAction::None;
                }
                view_.waitingPage = 0;
                view_.activePage = 0;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::RoomCreatedEvent>) {
                view_.pendingAction = PendingLobbyAction::None;
                if (value.room.visibility == "hidden" &&
                    value.room.roomCode) {
                    view_.hiddenRoomCode = *value.room.roomCode;
                    view_.screen = LobbyScreen::HiddenRoomWaiting;
                } else {
                    view_.screen = LobbyScreen::WaitingRoom;
                }
                view_.modal = LobbyModal::None;
                view_.pendingRoomId = value.room.roomId;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::GameStartedEvent>) {
                view_.roomReady = RoomReadyView{
                    value.roomId,
                    value.color,
                    value.opponent.username};
                view_.screen = LobbyScreen::RoomReady;
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::WatchingGameEvent>) {
                view_.spectator = SpectatorView{
                    value.roomId,
                    value.white.username,
                    value.black.username,
                    value.spectatorCount};
                view_.screen = LobbyScreen::SpectatorPlaceholder;
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::OpponentDisconnectedEvent>) {
                view_.visibleError = "The opponent disconnected.";
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::RoomStatusEvent>) {
                if (value.status == "removed") {
                    view_.screen = LobbyScreen::Lobby;
                    view_.visibleError =
                        "The room is no longer available.";
                    view_.pendingRoomId.clear();
                    view_.spectator.reset();
                }
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::ProtocolErrorEvent>) {
                view_.visibleError = value.message;
                view_.pendingAction = PendingLobbyAction::None;
            }
        },
        event);
}

void LobbyApplicationState::showError(const std::string& message) {
    view_.visibleError = message;
    view_.pendingAction = PendingLobbyAction::None;
}

void LobbyApplicationState::clearPending() {
    view_.pendingAction = PendingLobbyAction::None;
}

void LobbyApplicationState::resetForLogout() {
    view_ = LobbyViewModel{};
}

LobbyViewModel& LobbyApplicationState::editableForInput() {
    return view_;
}

std::string connectionStatusText(
    ctd::network::WebSocketConnectionState state) {
    switch (state) {
        case ctd::network::WebSocketConnectionState::Connecting:
            return "CONNECTING";
        case ctd::network::WebSocketConnectionState::Connected:
            return "CONNECTED";
        case ctd::network::WebSocketConnectionState::Closing:
            return "CLOSING";
        case ctd::network::WebSocketConnectionState::Failed:
            return "DISCONNECTED";
        case ctd::network::WebSocketConnectionState::Disconnected:
        default:
            return "DISCONNECTED";
    }
}

}  // namespace ctd::lobby
