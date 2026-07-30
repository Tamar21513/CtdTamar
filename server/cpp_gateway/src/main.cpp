#include "Config/GatewayConfig.hpp"
#include "App/GatewayApp.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        const auto config =
            ctd::gateway::GatewayConfig::fromEnvironment();
        std::cout << "CTD C++ Gateway listening on "
                  << config.bindAddress << ':' << config.port << '\n';
        ctd::gateway::GatewayApp(config).run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Gateway startup failed: " << error.what() << '\n';
        return 1;
    }
}
