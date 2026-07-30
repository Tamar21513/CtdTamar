#pragma once
#include "Auth/AuthService.hpp"
#include "Database/UserRepository.hpp"
#include "Redis/SessionStore.hpp"
#include "Rooms/RoomManager.hpp"
#include "WebSocket/WebSocketHub.hpp"
#include "Config/GatewayConfig.hpp"

namespace ctd::gateway {
struct GatewayState {
    UserRepository users;
    SessionStore sessions;
    PasswordHasher passwords;
    AuthService auth;
    RoomManager rooms;
    WebSocketHub hub;
    std::string cookieName;
    int sessionTtlSeconds;
    bool secureCookie;
    GatewayState(const GatewayConfig& config);
};
}
