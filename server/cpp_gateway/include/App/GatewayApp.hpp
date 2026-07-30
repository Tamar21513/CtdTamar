#pragma once
#include "Config/GatewayConfig.hpp"
namespace ctd::gateway {
class GatewayApp {
public:
    explicit GatewayApp(GatewayConfig config);
    void run();
private:
    GatewayConfig config_;
};
}
