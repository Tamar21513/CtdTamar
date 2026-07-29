#include "Network/SessionCookieJar.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace ctd::network {
namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    return first < last ? std::string(first, last) : std::string{};
}

}  // namespace

void SessionCookieJar::acceptSetCookie(const std::string& value) {
    const auto separator = value.find(';');
    const auto cookie = trim(value.substr(0, separator));
    const auto equals = cookie.find('=');
    if (equals == std::string::npos) {
        return;
    }
    const auto name = trim(cookie.substr(0, equals));
    const auto session = trim(cookie.substr(equals + 1));
    if (name != "ctd_session") {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (session.empty()) {
        sessionValue_.reset();
    } else {
        sessionValue_ = session;
    }
}

void SessionCookieJar::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessionValue_.reset();
}

bool SessionCookieJar::hasSession() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessionValue_.has_value();
}

std::optional<std::string> SessionCookieJar::cookieHeader() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sessionValue_) {
        return std::nullopt;
    }
    return "ctd_session=" + *sessionValue_;
}

std::string SessionCookieJar::debugDescription() const {
    return hasSession() ? "SessionCookieJar(authenticated)"
                        : "SessionCookieJar(empty)";
}

}  // namespace ctd::network
