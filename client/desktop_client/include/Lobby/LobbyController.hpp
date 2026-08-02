#pragma once

#include "Lobby/LobbyAction.hpp"
#include "Lobby/LobbyApplicationState.hpp"
#include "Lobby/LobbyTransport.hpp"

#include <chrono>
#include <future>
#include <optional>
#include <string>

namespace ctd::lobby {

class LobbyController {
public:
    LobbyController(
        LobbyApplicationState& state,
        ILobbyTransport& transport);
    ~LobbyController();

    void handle(const LobbyAction& action);
    void inputCharacter(char value);
    void backspace();
    void handleTab();
    void handleEnter();
    void gameBoardClick(int row, int col);
    void gameBoardJump(int row, int col);
    void tick();
    bool hasAuthenticationTask() const;

private:
    struct AuthenticationOutcome {
        ctd::network::AuthResult result;
        std::string username;
    };

    void beginAuthentication(bool registration);
    void finishAuthentication(AuthenticationOutcome outcome);
    void consumeNetworkEvents();
    void updateLocalCountdowns();
    void applyTransportEvent(
        const ctd::network::WebSocketEvent& event);
    bool beginRoomAction(
        PendingLobbyAction pending,
        const std::string& roomId = {});
    static std::string normalizeHiddenCode(const std::string& value);
    static std::string trimRoomName(const std::string& value);
    static std::string authErrorMessage(
        const ctd::network::AuthResult& result,
        bool registration);

    LobbyApplicationState& state_;
    ILobbyTransport& transport_;
    std::string password_;
    std::string confirmPassword_;
    std::future<AuthenticationOutcome> authenticationTask_;
    bool authenticationTaskActive_ = false;
    bool disconnectCountdownActive_ = false;
    std::chrono::steady_clock::time_point disconnectCountdownStart_;
    int disconnectCountdownInitialSeconds_ = 0;
    bool resumeCountdownActive_ = false;
    std::chrono::steady_clock::time_point resumeCountdownStart_;
    std::string resumeMessageText_;
};

}  // namespace ctd::lobby
