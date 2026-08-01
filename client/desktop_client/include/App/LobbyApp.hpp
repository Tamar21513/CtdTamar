#pragma once

#include "Network/ClientTransportConfig.hpp"

namespace ctd::lobby {

class LobbyApp {
public:
    void run(ctd::network::ClientTransportConfig config = {});
};

}  // namespace ctd::lobby
