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

MatchHistoryResult LobbyClient::getMatchHistory() {
    return apiClient_.getMatchHistory();
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

bool LobbyClient::createRoom(
    const std::string& name,
    bool hidden) {
    return websocketClient_.sendText(
        LobbyProtocol::createRoom(name, hidden));
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

bool LobbyClient::leaveSpectator(const std::string& roomId) {
    return websocketClient_.sendText(
        LobbyProtocol::leaveSpectator(roomId));
}

bool LobbyClient::findMatch() {
    return websocketClient_.sendText(LobbyProtocol::findMatch());
}

bool LobbyClient::cancelFindMatch() {
    return websocketClient_.sendText(
        LobbyProtocol::cancelFindMatch());
}

bool LobbyClient::sendMove(
    const std::string& roomId,
    unsigned long long sequence,
    int sourceRow,
    int sourceCol,
    int destinationRow,
    int destinationCol) {
    return websocketClient_.sendText(
        LobbyProtocol::moveRequest(
            roomId,
            sequence,
            sourceRow,
            sourceCol,
            destinationRow,
            destinationCol));
}

bool LobbyClient::sendJump(
    const std::string& roomId,
    unsigned long long sequence,
    int row,
    int col) {
    return websocketClient_.sendText(
        LobbyProtocol::jumpRequest(
            roomId, sequence, row, col));
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
