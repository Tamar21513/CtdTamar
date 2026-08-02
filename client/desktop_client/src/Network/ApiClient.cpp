#include "Network/ApiClient.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ctd::network {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using tcp = asio::ip::tcp;

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string port;
    std::string target;
};

ParsedUrl parseUrl(const std::string& url) {
    const auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        throw std::invalid_argument("URL is missing a scheme");
    }
    ParsedUrl parsed;
    parsed.scheme = url.substr(0, schemeEnd);
    const auto authorityStart = schemeEnd + 3;
    const auto pathStart = url.find('/', authorityStart);
    const auto authority = url.substr(
        authorityStart, pathStart - authorityStart);
    parsed.target = pathStart == std::string::npos
        ? "/" : url.substr(pathStart);
    const auto colon = authority.rfind(':');
    if (colon == std::string::npos) {
        parsed.host = authority;
        parsed.port = parsed.scheme == "https" ? "443" : "80";
    } else {
        parsed.host = authority.substr(0, colon);
        parsed.port = authority.substr(colon + 1);
    }
    if (parsed.host.empty() || parsed.port.empty()) {
        throw std::invalid_argument("URL authority is invalid");
    }
    return parsed;
}

std::string headerValue(
    const HttpResponse& response,
    const std::string& expectedName) {
    for (const auto& [name, value] : response.headers) {
        if (name.size() == expectedName.size() &&
            std::equal(
                name.begin(), name.end(), expectedName.begin(),
                [](char left, char right) {
                    return std::tolower(static_cast<unsigned char>(left)) ==
                        std::tolower(static_cast<unsigned char>(right));
                })) {
            return value;
        }
    }
    return {};
}

std::string jsonString(
    const json::object& object,
    const char* key) {
    const auto* value = object.if_contains(key);
    if (!value || !value->is_string()) {
        throw std::invalid_argument("Required JSON string is missing");
    }
    return std::string(value->as_string());
}

}  // namespace

HttpResponse BeastHttpTransport::send(const HttpRequest& request) {
    HttpResponse output;
    try {
        const auto parsed = parseUrl(request.url);
        if (parsed.scheme != "http") {
            output.transportError =
                "Only local HTTP is supported by this transport";
            return output;
        }
        asio::io_context context;
        tcp::resolver resolver(context);
        beast::tcp_stream stream(context);
        stream.connect(resolver.resolve(parsed.host, parsed.port));

        http::request<http::string_body> outgoing{
            request.method == HttpMethod::Post
                ? http::verb::post : http::verb::get,
            parsed.target,
            11};
        outgoing.set(http::field::host, parsed.host);
        outgoing.set(http::field::user_agent, "CTD-Native-Client/1");
        for (const auto& [name, value] : request.headers) {
            outgoing.set(name, value);
        }
        outgoing.body() = request.body;
        outgoing.prepare_payload();
        http::write(stream, outgoing);

        beast::flat_buffer buffer;
        http::response<http::string_body> incoming;
        http::read(stream, buffer, incoming);
        output.statusCode = incoming.result_int();
        output.body = std::move(incoming.body());
        for (const auto& field : incoming.base()) {
            output.headers.emplace_back(
                std::string(field.name_string()),
                std::string(field.value()));
        }
        beast::error_code ignored;
        stream.socket().shutdown(tcp::socket::shutdown_both, ignored);
    } catch (const std::exception& error) {
        output.transportError = error.what();
    }
    return output;
}

ApiClient::ApiClient(
    ClientTransportConfig config,
    SessionCookieJar& cookieJar,
    std::shared_ptr<IHttpTransport> transport)
    : config_(std::move(config)),
      cookieJar_(cookieJar),
      transport_(std::move(transport)) {}

HttpResponse ApiClient::send(
    HttpMethod method,
    const std::string& path,
    const std::string& body) {
    HttpRequest request{method, config_.apiBaseUrl + path, body, {}};
    if (!body.empty()) {
        request.headers.emplace_back(
            "Content-Type", "application/json");
    }
    if (const auto cookie = cookieJar_.cookieHeader()) {
        request.headers.emplace_back("Cookie", *cookie);
    }
    return transport_->send(request);
}

AuthResult ApiClient::parseAuthResponse(
    const HttpResponse& response) const {
    AuthResult result;
    result.statusCode = response.statusCode;
    if (!response.transportError.empty()) {
        result.kind = AuthResultKind::TransportError;
        result.message = response.transportError;
        return result;
    }
    try {
        const auto value = json::parse(response.body);
        if (response.statusCode >= 200 && response.statusCode < 300) {
            const auto& object = value.as_object();
            result.user = AuthenticatedUser{
                jsonString(object, "id"),
                jsonString(object, "username"),
                jsonString(object, "created_at")};
            result.kind = AuthResultKind::Success;
            return result;
        }
        if (value.is_object()) {
            const auto& object = value.as_object();
            if (const auto* detail = object.if_contains("detail");
                detail && detail->is_string()) {
                result.message = std::string(detail->as_string());
            }
        }
    } catch (const std::exception&) {
        result.kind = AuthResultKind::InvalidResponse;
        result.message = "Server returned an invalid response";
        return result;
    }
    if (response.statusCode == 401) {
        result.kind = AuthResultKind::InvalidCredentials;
        result.errorCode = "invalid_credentials";
    } else if (response.statusCode == 409) {
        result.kind = AuthResultKind::AlreadyRegistered;
        result.errorCode = "username_already_registered";
    } else if (response.statusCode == 422) {
        result.kind = AuthResultKind::ValidationError;
        result.errorCode = "validation_error";
    } else {
        result.kind = AuthResultKind::ServerError;
        result.errorCode = "server_error";
    }
    return result;
}

AuthResult ApiClient::credentialsRequest(
    const std::string& path,
    const std::string& username,
    const std::string& password) {
    json::object body{
        {"username", username},
        {"password", password}};
    const auto response = send(
        HttpMethod::Post, path, json::serialize(body));
    if (path == "/auth/login") {
        if (response.statusCode == 200) {
            const auto setCookie = headerValue(response, "Set-Cookie");
            if (!setCookie.empty()) {
                cookieJar_.acceptSetCookie(setCookie);
            }
        } else {
            cookieJar_.clear();
        }
    }
    auto result = parseAuthResponse(response);
    if (path == "/auth/login" &&
        result.succeeded() && !cookieJar_.hasSession()) {
        result.kind = AuthResultKind::InvalidResponse;
        result.user.reset();
        result.message = "Login response did not establish a session";
    }
    return result;
}

AuthResult ApiClient::registerUser(
    const std::string& username,
    const std::string& password) {
    return credentialsRequest("/auth/register", username, password);
}

AuthResult ApiClient::login(
    const std::string& username,
    const std::string& password) {
    return credentialsRequest("/auth/login", username, password);
}

AuthResult ApiClient::me() {
    const auto response = send(HttpMethod::Get, "/auth/me");
    auto result = parseAuthResponse(response);
    if (response.statusCode == 401) {
        cookieJar_.clear();
        result.kind = AuthResultKind::Unauthorized;
        result.errorCode = "unauthorized";
    }
    return result;
}

MatchHistoryResult ApiClient::getMatchHistory() {
    const auto response = send(HttpMethod::Get, "/matches/history");
    MatchHistoryResult result;
    if (!response.transportError.empty()) {
        result.message = response.transportError;
        return result;
    }
    if (response.statusCode == 401) {
        cookieJar_.clear();
        result.message = "Not authenticated";
        return result;
    }
    if (response.statusCode < 200 || response.statusCode >= 300) {
        result.message = "Failed to load match history";
        return result;
    }
    try {
        const auto value = json::parse(response.body);
        for (const auto& item : value.as_array()) {
            const auto& object = item.as_object();
            result.entries.push_back(MatchHistoryEntry{
                jsonString(object, "opponent"),
                jsonString(object, "color"),
                jsonString(object, "result"),
                static_cast<int>(
                    object.at("rating_before").as_int64()),
                static_cast<int>(
                    object.at("rating_after").as_int64()),
                jsonString(object, "reason"),
                jsonString(object, "ended_at")});
        }
        result.succeeded = true;
    } catch (const std::exception&) {
        result.message = "Malformed match history response";
    }
    return result;
}

AuthResult ApiClient::logout() {
    const auto response = send(HttpMethod::Post, "/auth/logout");
    cookieJar_.clear();
    if (!response.transportError.empty()) {
        return {AuthResultKind::TransportError, 0, std::nullopt, {},
                response.transportError};
    }
    if (response.statusCode == 204) {
        return {AuthResultKind::Success, 204};
    }
    return {AuthResultKind::ServerError, response.statusCode,
            std::nullopt, "server_error", "Logout failed"};
}

}  // namespace ctd::network
