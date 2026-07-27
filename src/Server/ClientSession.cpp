#include "../../include/Server/ClientSession.hpp"

#include <utility>

ClientSession::ClientSession(
    int clientId,
    PieceColor assignedColor,
    std::string username,
    TcpConnection connection
) :
    clientId(clientId),
    assignedColor(assignedColor),
    username(std::move(username)),
    connection(std::move(connection)),
    connected(true) {
}

int ClientSession::getClientId() const {
    return clientId;
}

PieceColor
ClientSession::getAssignedColor() const {
    return assignedColor;
}

bool ClientSession::isConnected() const {
    return connected.load();
}

TcpConnection&
ClientSession::getConnection() {
    return connection;
}

void ClientSession::sendMessage(
    const std::string& message
) {
    std::lock_guard<std::mutex> lock(
        sendMutex
    );

    if (!connected.load()) {
        return;
    }

    connection.sendMessage(message);
}

void ClientSession::disconnect() {
    if (!connected.exchange(false)) {
        return;
    }

    std::lock_guard<std::mutex> lock(
        sendMutex
    );

    connection.close();
}

const std::string& ClientSession::getUsername() const {
    return username;
}
