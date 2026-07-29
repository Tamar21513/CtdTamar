#pragma once

#include "Network/LobbyClient.hpp"

#include <optional>
#include <string>

namespace ctd::lobby {

class ILobbyTransport {
public:
    virtual ~ILobbyTransport() = default;

    virtual ctd::network::AuthResult registerUser(
        const std::string& username,
        const std::string& password) = 0;
    virtual ctd::network::AuthResult login(
        const std::string& username,
        const std::string& password) = 0;
    virtual ctd::network::AuthResult logout() = 0;
    virtual bool connectRealtime() = 0;
    virtual void disconnectRealtime() = 0;
    virtual ctd::network::WebSocketConnectionState connectionState()
        const = 0;
    virtual bool subscribeLobby() = 0;
    virtual bool requestLobby() = 0;
    virtual bool createPublicRoom() = 0;
    virtual bool createHiddenRoom() = 0;
    virtual bool joinPublicRoom(const std::string& roomId) = 0;
    virtual bool joinHiddenRoom(const std::string& roomCode) = 0;
    virtual bool watchRoom(const std::string& roomId) = 0;
    virtual std::optional<ctd::network::LobbyClientEvent> pollEvent() = 0;
};

class LobbyTransport final : public ILobbyTransport {
public:
    explicit LobbyTransport(
        ctd::network::ClientTransportConfig config = {});

    ctd::network::AuthResult registerUser(
        const std::string& username,
        const std::string& password) override;
    ctd::network::AuthResult login(
        const std::string& username,
        const std::string& password) override;
    ctd::network::AuthResult logout() override;
    bool connectRealtime() override;
    void disconnectRealtime() override;
    ctd::network::WebSocketConnectionState connectionState()
        const override;
    bool subscribeLobby() override;
    bool requestLobby() override;
    bool createPublicRoom() override;
    bool createHiddenRoom() override;
    bool joinPublicRoom(const std::string& roomId) override;
    bool joinHiddenRoom(const std::string& roomCode) override;
    bool watchRoom(const std::string& roomId) override;
    std::optional<ctd::network::LobbyClientEvent> pollEvent() override;

private:
    ctd::network::LobbyClient client_;
};

}  // namespace ctd::lobby
