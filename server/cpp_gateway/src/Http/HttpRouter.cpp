#include "Http/HttpRouter.hpp"
#include <boost/json.hpp>

namespace ctd::gateway {
namespace json = boost::json;
namespace {
http::response<http::string_body> response(
    http::status status, json::value body) {
    http::response<http::string_body> result{status, 11};
    result.set(http::field::content_type, "application/json");
    result.body() = json::serialize(body);
    result.prepare_payload();
    return result;
}
json::object safeUser(const User& user) {
    return {{"id", user.id}, {"username", user.username},
            {"created_at", user.createdAt}};
}

std::pair<std::string, std::string> credentials(
    const std::string& body) {
    boost::system::error_code error;
    const auto parsed = json::parse(body, error);
    if (error || !parsed.is_object()) {
        throw std::invalid_argument("invalid request");
    }
    const auto& object = parsed.as_object();
    const auto* username = object.if_contains("username");
    const auto* password = object.if_contains("password");
    if (!username || !username->is_string() ||
        !password || !password->is_string()) {
        throw std::invalid_argument("invalid request");
    }
    return {
        std::string(username->as_string()),
        std::string(password->as_string())};
}
}
std::string HttpRouter::cookie(
    const http::request<http::string_body>& request,
    const std::string& name) {
    const auto text = std::string(request[http::field::cookie]);
    const std::string prefix = name + "=";
    for (std::size_t start = 0; start < text.size();) {
        const auto end = text.find(';', start);
        auto part = text.substr(start, end - start);
        while (!part.empty() && part.front() == ' ') part.erase(part.begin());
        if (part.rfind(prefix, 0) == 0) return part.substr(prefix.size());
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}
std::optional<User> HttpRouter::authenticate(
    const http::request<http::string_body>& request) {
    return state_.auth.currentUser(cookie(request, state_.cookieName));
}
http::response<http::string_body> HttpRouter::route(
    const http::request<http::string_body>& request) {
    try {
        const std::string target(request.target());
        if (request.method() == http::verb::get && target == "/health") {
            const bool postgres = state_.users.healthy();
            const bool redis = state_.sessions.healthy();
            return response(
                postgres && redis ? http::status::ok
                                  : http::status::service_unavailable,
                {{"cpp_gateway", postgres && redis ? "healthy" : "unhealthy"},
                 {"postgresql", postgres ? "healthy" : "unhealthy"},
                 {"redis", redis ? "healthy" : "unhealthy"}});
        }
        if (request.method() == http::verb::get && target == "/auth/me") {
            const auto user = authenticate(request);
            return user ? response(http::status::ok, safeUser(*user))
                        : response(http::status::unauthorized,
                                   {{"detail", "Not authenticated"}});
        }
        if (request.method() == http::verb::post &&
            target == "/auth/logout") {
            state_.auth.logout(cookie(request, state_.cookieName));
            auto result = response(http::status::no_content, nullptr);
            result.body().clear(); result.prepare_payload();
            result.set(http::field::set_cookie,
                state_.cookieName +
                "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
            return result;
        }
        if (request.method() == http::verb::post &&
            (target == "/auth/register" || target == "/auth/login")) {
            const auto [username, password] = credentials(request.body());
            if (target == "/auth/register") {
                try {
                    return response(
                        http::status::created,
                        safeUser(state_.auth.registerUser(username, password)));
                } catch (const DuplicateUsername&) {
                    return response(http::status::conflict,
                        {{"detail", "Username is already registered"}});
                }
            }
            const auto login = state_.auth.login(username, password);
            if (!login) return response(http::status::unauthorized,
                {{"detail", "Invalid username or password"}});
            auto result = response(http::status::ok, safeUser(login->user));
            result.set(http::field::set_cookie,
                state_.cookieName + "=" + login->sessionToken +
                "; Path=/; Max-Age=" +
                std::to_string(state_.sessionTtlSeconds) +
                "; HttpOnly; SameSite=Lax" +
                (state_.secureCookie ? "; Secure" : ""));
            return result;
        }
        return response(http::status::not_found, {{"detail", "Not found"}});
    } catch (const std::invalid_argument&) {
        return response(http::status::unprocessable_entity,
            {{"detail", "Invalid request"}});
    } catch (const std::exception&) {
        return response(http::status::service_unavailable,
            {{"detail", "Service unavailable"}});
    }
}
}
