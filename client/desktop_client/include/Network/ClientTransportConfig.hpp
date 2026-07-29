#pragma once

#include <string>

namespace ctd::network {

struct ClientTransportConfig {
    std::string apiBaseUrl = "http://127.0.0.1:8000";
    std::string websocketUrl = "ws://127.0.0.1:8000/ws";
};

}  // namespace ctd::network
