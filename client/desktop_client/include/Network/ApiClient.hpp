#pragma once

#include "Network/ClientTransportConfig.hpp"
#include "Network/SessionCookieJar.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ctd::network {

enum class HttpMethod { Get, Post };

struct HttpRequest {
    HttpMethod method = HttpMethod::Get;
    std::string url;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string transportError;
};

class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;
    virtual HttpResponse send(const HttpRequest& request) = 0;
};

class BeastHttpTransport final : public IHttpTransport {
public:
    HttpResponse send(const HttpRequest& request) override;
};

struct AuthenticatedUser {
    std::string id;
    std::string username;
    std::string createdAt;
};

struct MatchHistoryEntry {
    std::string opponent;
    std::string color;
    std::string result;
    int ratingBefore = 1200;
    int ratingAfter = 1200;
    std::string reason;
    std::string endedAt;
};

struct MatchHistoryResult {
    bool succeeded = false;
    std::vector<MatchHistoryEntry> entries;
    std::string message;
};

enum class AuthResultKind {
    Success,
    InvalidCredentials,
    AlreadyRegistered,
    Unauthorized,
    ValidationError,
    ServerError,
    TransportError,
    InvalidResponse
};

struct AuthResult {
    AuthResultKind kind = AuthResultKind::InvalidResponse;
    int statusCode = 0;
    std::optional<AuthenticatedUser> user;
    std::string errorCode;
    std::string message;

    bool succeeded() const { return kind == AuthResultKind::Success; }
};

class ApiClient {
public:
    ApiClient(
        ClientTransportConfig config,
        SessionCookieJar& cookieJar,
        std::shared_ptr<IHttpTransport> transport =
            std::make_shared<BeastHttpTransport>());

    AuthResult registerUser(
        const std::string& username,
        const std::string& password);
    AuthResult login(
        const std::string& username,
        const std::string& password);
    AuthResult me();
    AuthResult logout();
    MatchHistoryResult getMatchHistory();

private:
    AuthResult credentialsRequest(
        const std::string& path,
        const std::string& username,
        const std::string& password);
    AuthResult parseAuthResponse(const HttpResponse& response) const;
    HttpResponse send(
        HttpMethod method,
        const std::string& path,
        const std::string& body = {});

    ClientTransportConfig config_;
    SessionCookieJar& cookieJar_;
    std::shared_ptr<IHttpTransport> transport_;
};

}  // namespace ctd::network
