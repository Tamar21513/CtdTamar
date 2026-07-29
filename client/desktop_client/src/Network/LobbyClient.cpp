#include "Network/LobbyClient.hpp"

namespace ctd::network {

LobbyClient::LobbyClient(
    ClientTransportConfig config,
    std::shared_ptr<IHttpTransport> httpTransport)
    : config_(std::move(config)),
      apiClient_(config_, cookieJar_, std::move(httpTransport)),
      websocketClient_(cookieJar_) {}

LobbyClient::~LobbyClient() {
    disconnectRealtime();
}

AuthResult LobbyClient::registerUser(
    const std::string& username,
    const std::string& password) {
    return apiClient_.registerUser(username, password);
}

AuthResult LobbyClient::login(
    const std::string& username,
    const std::string& password) {
    auto loginResult = apiClient_.login(username, password);
    if (!loginResult.succeeded()) {
        return loginResult;
    }
    return apiClient_.me();
}

AuthResult LobbyClient::currentUser() {
    return apiClient_.me();
}

AuthResult LobbyClient::logout() {
    disconnectRealtime();
    return apiClient_.logout();
}

bool LobbyClient::connectRealtime() {
    if (!cookieJar_.hasSession()) {
        return false;
    }
    return websocketClient_.connect(config_.websocketUrl);
}

void LobbyClient::disconnectRealtime() {
    websocketClient_.disconnect();
}

WebSocketConnectionState LobbyClient::connectionState() const {
    return websocketClient_.state();
}

bool LobbyClient::ping() {
    return websocketClient_.sendText(LobbyProtocol::ping());
}

bool LobbyClient::subscribeLobby() {
    return websocketClient_.sendText(
        LobbyProtocol::subscribeLobby());
}

bool LobbyClient::requestLobby() {
    return websocketClient_.sendText(LobbyProtocol::getLobby());
}

bool LobbyClient::createPublicRoom() {
    return websocketClient_.sendText(
        LobbyProtocol::createRoom(false));
}

bool LobbyClient::createHiddenRoom() {
    return websocketClient_.sendText(
        LobbyProtocol::createRoom(true));
}

bool LobbyClient::joinPublicRoom(const std::string& roomId) {
    return websocketClient_.sendText(
        LobbyProtocol::joinRoom(roomId));
}

bool LobbyClient::joinHiddenRoom(const std::string& roomCode) {
    return websocketClient_.sendText(
        LobbyProtocol::joinHiddenRoom(roomCode));
}

bool LobbyClient::watchRoom(const std::string& roomId) {
    return websocketClient_.sendText(
        LobbyProtocol::watchGame(roomId));
}

std::optional<LobbyClientEvent> LobbyClient::pollEvent() {
    auto transport = websocketClient_.pollEvent();
    if (!transport) {
        return std::nullopt;
    }
    if (transport->kind != WebSocketEventKind::Text) {
        return LobbyClientEvent{
            std::nullopt, std::move(transport), {}};
    }
    auto parsed = LobbyProtocol::parse(transport->text);
    return LobbyClientEvent{
        std::move(parsed.event), std::nullopt, std::move(parsed.error)};
}

}  // namespace ctd::network
