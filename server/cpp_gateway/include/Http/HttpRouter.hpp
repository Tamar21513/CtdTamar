#pragma once
#include "App/GatewayState.hpp"
#include <boost/beast/http.hpp>

namespace ctd::gateway {
namespace http = boost::beast::http;
class HttpRouter {
public:
    explicit HttpRouter(GatewayState& state) : state_(state) {}
    http::response<http::string_body> route(
        const http::request<http::string_body>& request);
    std::optional<User> authenticate(
        const http::request<http::string_body>& request);
    static std::string cookie(
        const http::request<http::string_body>& request,
        const std::string& name);
private:
    GatewayState& state_;
};
}
