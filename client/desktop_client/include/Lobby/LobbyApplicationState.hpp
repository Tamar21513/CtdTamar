#pragma once

#include "Network/ApiClient.hpp"
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
    MatchHistory,
    SpectatorPlaceholder,
    Game,
    SpectatorGame,
    Error
};

enum class LobbyModal {
    None,
    CreateRoom,
    JoinHiddenRoom
};

enum class AuthenticationField {
    Username,
    Password,
    ConfirmPassword
};

enum class AuthenticationMode {
    Login,
    Register
};

enum class RoomVisibilitySelection {
    Public,
    Hidden
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
    FindMatch,
    Logout
};

struct SpectatorView {
    std::string roomId;
    std::string roomName;
    std::string whiteUsername;
    std::string blackUsername;
    int spectatorCount = 0;
};

struct RoomReadyView {
    std::string roomId;
    std::string roomName;
    std::string assignedColor;
    std::string opponentUsername;
    int opponentRating = 1200;
};
enum class MatchPhase {
    Loading,
    Countdown,
    Playing,
    Finished
};

struct AuthoritativeMatchView {
    std::string roomId;
    std::string assignedColor;
    std::string opponentUsername;
    unsigned long long revision = 0;
    unsigned long long nextSequence = 1;
    GameStateSnapshot snapshot;
    std::optional<Position> selectedSource;
    std::string moveStatus;
    bool spectator = false;
    MatchPhase phase = MatchPhase::Loading;
    std::string gameStartsAt;
    std::string countdownValue;
    int whiteRating = 1200;
    int blackRating = 1200;
    int opponentReconnectSecondsRemaining = 0;
    std::string reconnectStatusMessage;
    std::string reconnectStatusLine2;
};

struct LobbyViewModel {
    LobbyScreen screen = LobbyScreen::Authentication;
    LobbyModal modal = LobbyModal::None;
    AuthenticationField focusedField = AuthenticationField::Username;
    AuthenticationMode authMode = AuthenticationMode::Login;
    PendingLobbyAction pendingAction = PendingLobbyAction::None;
    ctd::network::WebSocketConnectionState connectionState =
        ctd::network::WebSocketConnectionState::Disconnected;
    std::string usernameInput;
    std::size_t passwordLength = 0;
    std::size_t confirmPasswordLength = 0;
    std::string authenticatedUsername;
    int authenticatedUserRating = 1200;
    std::string visibleError;
    std::string statusMessage;
    std::string roomNameInput;
    std::string currentRoomName;
    RoomVisibilitySelection selectedRoomVisibility =
        RoomVisibilitySelection::Public;
    std::string hiddenRoomCodeInput;
    std::string hiddenRoomCode;
    std::string pendingRoomId;
    std::vector<ctd::network::LobbyRoom> waitingRooms;
    std::vector<ctd::network::LobbyRoom> activeRooms;
    std::optional<SpectatorView> spectator;
    std::optional<RoomReadyView> roomReady;
    std::optional<AuthoritativeMatchView> match;
    int waitingPage = 0;
    int activePage = 0;
    std::vector<ctd::network::MatchHistoryEntry> matchHistory;
    std::string matchHistoryError;

    bool networkActionsEnabled() const;
};

struct GameplayPresentation {
    std::string statusLine1;
    std::string statusLine2;
    bool showSpectatorBackButton = false;
};

GameplayPresentation gameplayPresentation(
    const LobbyViewModel& view);

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
