#pragma once

#include <cstdint>
#include <string>

namespace ctd::gateway {

struct GatewayConfig {
    std::string bindAddress = "0.0.0.0";
    std::uint16_t port = 8000;
    std::string postgresConnection;
    std::string redisUri = "tcp://127.0.0.1:6379/0";
    std::string sessionCookieName = "ctd_session";
    int sessionTtlSeconds = 3600;
    bool secureCookie = false;

    static GatewayConfig fromEnvironment();
};

}  // namespace ctd::gateway
