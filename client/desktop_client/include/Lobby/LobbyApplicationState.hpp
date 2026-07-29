#pragma once

#include "Network/LobbyProtocol.hpp"
#include "Network/WebSocketClient.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ctd::lobby {

enum class LobbyScreen {
    Authentication,
    Connecting,
    Lobby,
    WaitingRoom,
    HiddenRoomWaiting,
    RoomReady,
    SpectatorPlaceholder,
    Error
};

enum class LobbyModal {
    None,
    CreateRoom,
    JoinHiddenRoom
};

enum class AuthenticationField {
    Username,
    Password
};

enum class PendingLobbyAction {
    None,
    Login,
    Register,
    CreatePublicRoom,
    CreateHiddenRoom,
    JoinPublicRoom,
    JoinHiddenRoom,
    WatchRoom,
    Logout
};

struct SpectatorView {
    std::string roomId;
    std::string whiteUsername;
    std::string blackUsername;
    int spectatorCount = 0;
};

struct RoomReadyView {
    std::string roomId;
    std::string assignedColor;
    std::string opponentUsername;
};

struct LobbyViewModel {
    LobbyScreen screen = LobbyScreen::Authentication;
    LobbyModal modal = LobbyModal::None;
    AuthenticationField focusedField = AuthenticationField::Username;
    PendingLobbyAction pendingAction = PendingLobbyAction::None;
    ctd::network::WebSocketConnectionState connectionState =
        ctd::network::WebSocketConnectionState::Disconnected;
    std::string usernameInput;
    std::size_t passwordLength = 0;
    std::string authenticatedUsername;
    std::string visibleError;
    std::string statusMessage;
    std::string hiddenRoomCodeInput;
    std::string hiddenRoomCode;
    std::string pendingRoomId;
    std::vector<ctd::network::LobbyRoom> waitingRooms;
    std::vector<ctd::network::LobbyRoom> activeRooms;
    std::optional<SpectatorView> spectator;
    std::optional<RoomReadyView> roomReady;
    int waitingPage = 0;
    int activePage = 0;

    bool networkActionsEnabled() const;
};

class LobbyApplicationState {
public:
    const LobbyViewModel& view() const;
    LobbyViewModel snapshot() const;

    void beginAuthentication(PendingLobbyAction action);
    void authenticationFailed(const std::string& message);
    void authenticationSucceeded(const std::string& username);
    void setConnectionState(
        ctd::network::WebSocketConnectionState state);
    void applyEvent(const ctd::network::LobbyEvent& event);
    void showError(const std::string& message);
    void clearPending();
    void resetForLogout();

    LobbyViewModel& editableForInput();

private:
    LobbyViewModel view_;
};

std::string connectionStatusText(
    ctd::network::WebSocketConnectionState state);

}  // namespace ctd::lobby
