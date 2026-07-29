#pragma once

#include <mutex>
#include <optional>
#include <string>

namespace ctd::network {

class ApiClient;
class WebSocketClient;

class SessionCookieJar {
public:
    void acceptSetCookie(const std::string& value);
    void clear();
    bool hasSession() const;
    std::string debugDescription() const;

private:
    friend class ApiClient;
    friend class WebSocketClient;
    std::optional<std::string> cookieHeader() const;

    mutable std::mutex mutex_;
    std::optional<std::string> sessionValue_;
};

}  // namespace ctd::network
