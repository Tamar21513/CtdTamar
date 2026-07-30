#include "Config/GatewayConfig.hpp"

#include <cstdlib>
#include <stdexcept>

namespace ctd::gateway {
namespace {

std::string environment(const char* name, const std::string& fallback = {}) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : fallback;
}

int positiveInteger(const char* name, int fallback) {
    const auto text = environment(name);
    if (text.empty()) {
        return fallback;
    }
    try {
        const int value = std::stoi(text);
        if (value < 1) {
            throw std::invalid_argument("not positive");
        }
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error(
            std::string("Invalid environment value: ") + name);
    }
}

}  // namespace

GatewayConfig GatewayConfig::fromEnvironment() {
    GatewayConfig config;
    config.bindAddress = environment("CPP_GATEWAY_BIND", "0.0.0.0");
    const int gatewayPort = positiveInteger("API_GATEWAY_PORT", 8000);
    if (gatewayPort > 65535) {
        throw std::runtime_error(
            "Invalid environment value: API_GATEWAY_PORT");
    }
    config.port = static_cast<std::uint16_t>(gatewayPort);
    const auto database = environment("POSTGRES_DB", "ctd");
    const auto user = environment("POSTGRES_USER", "ctd_local");
    const auto password = environment("POSTGRES_PASSWORD");
    const auto host = environment("POSTGRES_HOST", "postgres");
    const auto port = environment("POSTGRES_PORT", "5432");
    if (password.empty()) {
        throw std::runtime_error("POSTGRES_PASSWORD is required");
    }
    config.postgresConnection =
        "host=" + host + " port=" + port + " dbname=" + database +
        " user=" + user + " password=" + password;
    config.redisUri =
        "tcp://" + environment("REDIS_HOST", "redis") + ":" +
        environment("REDIS_PORT", "6379") + "/" +
        environment("REDIS_DB", "0");
    config.sessionCookieName =
        environment("SESSION_COOKIE_NAME", "ctd_session");
    config.sessionTtlSeconds =
        positiveInteger("SESSION_TTL_SECONDS", 3600);
    config.secureCookie =
        environment("SESSION_COOKIE_SECURE", "false") == "true";
    return config;
}

}  // namespace ctd::gateway
