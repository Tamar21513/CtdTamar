#include "App/GatewayApp.hpp"
#include "App/GatewayState.hpp"
#include "Http/HttpServer.hpp"
#include <boost/asio.hpp>
#include <thread>
#include <vector>

namespace ctd::gateway {
GatewayState::GatewayState(const GatewayConfig& config)
    : users(config.postgresConnection),
      sessions(config.redisUri, config.sessionTtlSeconds),
      passwords(),
      auth(users, sessions, passwords),
      cookieName(config.sessionCookieName),
      sessionTtlSeconds(config.sessionTtlSeconds),
      secureCookie(config.secureCookie) {}
GatewayApp::GatewayApp(GatewayConfig config) : config_(std::move(config)) {}
void GatewayApp::run() {
    boost::asio::io_context context;
    GatewayState state(config_);
    state.users.migrate();
    HttpServer server(context, config_.bindAddress, config_.port, state);
    boost::asio::signal_set signals(context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { context.stop(); });
    const auto count = std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    for (unsigned index = 1; index < count; ++index)
        workers.emplace_back([&] { context.run(); });
    context.run();
    for (auto& worker : workers) worker.join();
}
}
