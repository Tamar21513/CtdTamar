#pragma once

#include <string>

namespace ctd::network {

struct ClientTransportConfig {
    std::string apiBaseUrl = "http://127.0.0.1:8000";
    std::string websocketUrl = "ws://127.0.0.1:8000/ws";
};

inline ClientTransportConfig makeTransportConfig(
    const std::string& host,
    unsigned short port) {
    const std::string hostPort = host + ":" + std::to_string(port);
    return ClientTransportConfig{
        "http://" + hostPort,
        "ws://" + hostPort + "/ws"};
}

}  // namespace ctd::network
