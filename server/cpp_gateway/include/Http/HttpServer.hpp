#pragma once
#include "App/GatewayState.hpp"
#include <boost/asio.hpp>
#include <memory>

namespace ctd::gateway {
class HttpServer {
public:
    HttpServer(boost::asio::io_context& context,
               const std::string& address,
               unsigned short port,
               GatewayState& state);
private:
    class Listener;
    std::shared_ptr<Listener> listener_;
};
}
