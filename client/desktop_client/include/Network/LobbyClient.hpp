#pragma once

#include "Network/ApiClient.hpp"
#include "Network/LobbyProtocol.hpp"
#include "Network/WebSocketClient.hpp"

#include <memory>
#include <optional>
#include <string>

namespace ctd::network {

struct LobbyClientEvent {
    std::optional<LobbyEvent> protocolEvent;
    std::optional<WebSocketEvent> transportEvent;
    std::string parseError;
};

class LobbyClient {
public:
    explicit LobbyClient(
        ClientTransportConfig config = {},
        std::shared_ptr<IHttpTransport> httpTransport =
            std::make_shared<BeastHttpTransport>());
    ~LobbyClient();

    AuthResult registerUser(
        const std::string& username,
        const std::string& password);
    AuthResult login(
        const std::string& username,
        const std::string& password);
    AuthResult currentUser();
    AuthResult logout();

    bool connectRealtime();
    void disconnectRealtime();
    WebSocketConnectionState connectionState() const;

    bool ping();
    bool subscribeLobby();
    bool requestLobby();
    bool createPublicRoom();
    bool createHiddenRoom();
    bool joinPublicRoom(const std::string& roomId);
    bool joinHiddenRoom(const std::string& roomCode);
    bool watchRoom(const std::string& roomId);
    std::optional<LobbyClientEvent> pollEvent();

private:
    ClientTransportConfig config_;
    SessionCookieJar cookieJar_;
    ApiClient apiClient_;
    WebSocketClient websocketClient_;
};

}  // namespace ctd::network
