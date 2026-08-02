#include "Lobby/LobbyApplicationState.hpp"

#include "Logging/FileLogger.hpp"

#include <algorithm>
#include <type_traits>

namespace ctd::lobby {

GameplayPresentation gameplayPresentation(
    const LobbyViewModel& view) {
    const bool showBackButton =
        view.screen == LobbyScreen::SpectatorGame ||
        (view.screen == LobbyScreen::Game &&
         view.match && view.match->snapshot.gameOver);
    return {"", "", showBackButton};
}

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
    view_.confirmPasswordLength = 0;
}

void LobbyApplicationState::authenticationSucceeded(
    const std::string& username) {
    view_.screen = LobbyScreen::Connecting;
    view_.authenticatedUsername = username;
    view_.usernameInput = username;
    view_.passwordLength = 0;
    view_.confirmPasswordLength = 0;
    view_.visibleError.clear();
    view_.statusMessage = "Connecting to lobby...";
    view_.pendingAction = PendingLobbyAction::None;
}

void LobbyApplicationState::setConnectionState(
    ctd::network::WebSocketConnectionState state) {
    if (state != view_.connectionState) {
        ctd::logging::defaultLogger().info(
            "connection state changed from=" +
            connectionStatusText(view_.connectionState) +
            " to=" + connectionStatusText(state));
    }
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
                view_.authenticatedUserRating = value.user.rating;
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
                ctd::logging::defaultLogger().info(
                    "room created id=" + value.room.roomId +
                    " name=" + value.room.name +
                    " visibility=" + value.room.visibility);
                view_.pendingAction = PendingLobbyAction::None;
                view_.currentRoomName = value.room.name;
                view_.roomNameInput.clear();
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
                ctd::logging::defaultLogger().info(
                    "room joined id=" + value.roomId +
                    " color=" + value.color);
                view_.currentRoomName = value.roomName;
                view_.roomReady = RoomReadyView{
                    value.roomId,
                    value.roomName,
                    value.color,
                    value.opponent.username,
                    value.opponent.rating};
                view_.screen = LobbyScreen::RoomReady;
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::WatchingGameEvent>) {
                ctd::logging::defaultLogger().info(
                    "room watch id=" + value.roomId);
                view_.spectator = SpectatorView{
                    value.roomId,
                    value.roomName,
                    value.white.username,
                    value.black.username,
                    value.spectatorCount};
                view_.screen = LobbyScreen::SpectatorPlaceholder;
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MatchReadyEvent>) {
                ctd::logging::defaultLogger().info(
                    "match started id=" + value.roomId +
                    " color=" + value.color);
                view_.match = AuthoritativeMatchView{
                    value.roomId,
                    value.color,
                    value.opponent,
                    value.revision,
                    1,
                    value.state,
                    std::nullopt,
                    {},
                    value.color == "spectator",
                    MatchPhase::Countdown,
                    value.gameStartsAt,
                    "3",
                    value.whiteRating,
                    value.blackRating};
                view_.pendingRoomId = value.roomId;
                view_.screen = value.color == "spectator"
                    ? LobbyScreen::SpectatorGame
                    : LobbyScreen::Game;
                view_.roomReady.reset();
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MatchSnapshotEvent>) {
                view_.match = AuthoritativeMatchView{
                    value.roomId,
                    {},
                    {},
                    value.revision,
                    1,
                    value.state,
                    std::nullopt,
                    {},
                    true,
                    MatchPhase::Playing,
                    {},
                    {}};
                view_.pendingRoomId = value.roomId;
                view_.screen = LobbyScreen::SpectatorGame;
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MatchCountdownEvent>) {
                if (view_.match &&
                    view_.match->roomId == value.roomId &&
                    view_.match->phase == MatchPhase::Countdown &&
                    view_.match->gameStartsAt == value.gameStartsAt) {
                    view_.match->countdownValue = value.value;
                }
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MatchStartedEvent>) {
                if (view_.match &&
                    view_.match->roomId == value.roomId) {
                    view_.match->revision = value.revision;
                    view_.match->snapshot = value.state;
                    view_.match->phase = MatchPhase::Playing;
                    view_.match->countdownValue.clear();
                }
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MatchCancelledEvent>) {
                ctd::logging::defaultLogger().warn(
                    "match cancelled id=" + value.roomId +
                    " reason=" + value.reason);
                if (view_.match &&
                    view_.match->roomId == value.roomId) {
                    view_.match.reset();
                    view_.screen = LobbyScreen::Lobby;
                    view_.visibleError =
                        "Match cancelled because a player disconnected.";
                }
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MatchStateEvent>) {
                if (view_.match &&
                    view_.match->roomId == value.roomId &&
                    value.revision > view_.match->revision) {
                    view_.match->revision = value.revision;
                    view_.match->snapshot = value.state;
                }
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MoveResultEvent>) {
                if (view_.match &&
                    view_.match->roomId == value.roomId) {
                    view_.match->moveStatus = value.accepted
                        ? ""
                        : "Move rejected: " + value.reason;
                }
            } else if constexpr (
                std::is_same_v<Event, ctd::network::OpponentDisconnectedEvent>) {
                ctd::logging::defaultLogger().warn(
                    "opponent disconnected id=" + value.roomId);
                view_.visibleError = "The opponent disconnected.";
                view_.pendingAction = PendingLobbyAction::None;
            } else if constexpr (
                std::is_same_v<Event, ctd::network::RoomStatusEvent>) {
                if (value.status == "removed") {
                    ctd::logging::defaultLogger().info(
                        "room removed id=" + value.roomId);
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
            } else if constexpr (
                std::is_same_v<Event, ctd::network::SearchingEvent>) {
                view_.pendingAction = PendingLobbyAction::FindMatch;
                view_.statusMessage = "Searching for an opponent...";
                view_.visibleError.clear();
            } else if constexpr (
                std::is_same_v<Event, ctd::network::MatchNotFoundEvent>) {
                ctd::logging::defaultLogger().warn(
                    "find_match timed out with no opponent");
                view_.pendingAction = PendingLobbyAction::None;
                view_.statusMessage.clear();
                view_.visibleError =
                    "No opponent was found. Try again.";
            } else if constexpr (
                std::is_same_v<
                    Event, ctd::network::FindMatchCancelledEvent>) {
                view_.pendingAction = PendingLobbyAction::None;
                view_.statusMessage.clear();
            } else if constexpr (
                std::is_same_v<Event, ctd::network::RatingUpdatedEvent>) {
                view_.authenticatedUserRating = value.rating;
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
