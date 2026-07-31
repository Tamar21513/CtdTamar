#include "Lobby/LobbyTransport.hpp"

namespace ctd::lobby {

LobbyTransport::LobbyTransport(
    ctd::network::ClientTransportConfig config)
    : client_(std::move(config)) {}

ctd::network::AuthResult LobbyTransport::registerUser(
    const std::string& username,
    const std::string& password) {
    return client_.registerUser(username, password);
}

ctd::network::AuthResult LobbyTransport::login(
    const std::string& username,
    const std::string& password) {
    return client_.login(username, password);
}

ctd::network::AuthResult LobbyTransport::logout() {
    return client_.logout();
}

bool LobbyTransport::connectRealtime() {
    return client_.connectRealtime();
}

void LobbyTransport::disconnectRealtime() {
    client_.disconnectRealtime();
}

ctd::network::WebSocketConnectionState
LobbyTransport::connectionState() const {
    return client_.connectionState();
}

bool LobbyTransport::subscribeLobby() {
    return client_.subscribeLobby();
}

bool LobbyTransport::requestLobby() {
    return client_.requestLobby();
}

bool LobbyTransport::createRoom(
    const std::string& name,
    bool hidden) {
    return client_.createRoom(name, hidden);
}

bool LobbyTransport::joinPublicRoom(const std::string& roomId) {
    return client_.joinPublicRoom(roomId);
}

bool LobbyTransport::joinHiddenRoom(const std::string& roomCode) {
    return client_.joinHiddenRoom(roomCode);
}

bool LobbyTransport::watchRoom(const std::string& roomId) {
    return client_.watchRoom(roomId);
}

bool LobbyTransport::leaveSpectator(
    const std::string& roomId) {
    return client_.leaveSpectator(roomId);
}

bool LobbyTransport::findMatch() {
    return client_.findMatch();
}

bool LobbyTransport::cancelFindMatch() {
    return client_.cancelFindMatch();
}

bool LobbyTransport::sendMove(
    const std::string& roomId,
    unsigned long long sequence,
    int sourceRow,
    int sourceCol,
    int destinationRow,
    int destinationCol) {
    return client_.sendMove(
        roomId,
        sequence,
        sourceRow,
        sourceCol,
        destinationRow,
        destinationCol);
}

bool LobbyTransport::sendJump(
    const std::string& roomId,
    unsigned long long sequence,
    int row,
    int col) {
    return client_.sendJump(roomId, sequence, row, col);
}

std::optional<ctd::network::LobbyClientEvent>
LobbyTransport::pollEvent() {
    return client_.pollEvent();
}

}  // namespace ctd::lobby
