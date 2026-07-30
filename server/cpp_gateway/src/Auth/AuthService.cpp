#include "Auth/AuthService.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ctd::gateway {

AuthService::AuthService(
    UserRepository& users,
    SessionStore& sessions,
    PasswordHasher& passwords)
    : users_(users), sessions_(sessions), passwords_(passwords) {}

std::string AuthService::validateUsername(const std::string& username) {
    const auto first = username.find_first_not_of(" \t\r\n");
    const auto last = username.find_last_not_of(" \t\r\n");
    const std::string value = first == std::string::npos
        ? "" : username.substr(first, last - first + 1);
    if (value.size() < 3 || value.size() > 32 ||
        !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '_' || c == '-' || c == '.';
        })) {
        throw std::invalid_argument("invalid username");
    }
    return value;
}

bool AuthService::validPassword(const std::string& password) {
    return password.size() >= 10 && password.size() <= 128 &&
        std::any_of(password.begin(), password.end(), [](unsigned char c) {
            return !std::isspace(c);
        });
}

std::string AuthService::normalizeUsername(
    const std::string& username) {
    std::string result = validateUsername(username);
    std::transform(
        result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
    return result;
}

User AuthService::registerUser(
    const std::string& username,
    const std::string& password) {
    const auto display = validateUsername(username);
    if (!validPassword(password)) {
        throw std::invalid_argument("invalid password");
    }
    return users_.create(
        display,
        normalizeUsername(display),
        passwords_.hash(password));
}

std::optional<LoginResult> AuthService::login(
    const std::string& username,
    const std::string& password) {
    if (!validPassword(password)) {
        return std::nullopt;
    }
    std::optional<User> user;
    try {
        user = users_.findByUsername(normalizeUsername(username));
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
    if (!user || !passwords_.verify(user->passwordHash, password)) {
        return std::nullopt;
    }
    return LoginResult{*user, sessions_.create(user->id)};
}

std::optional<User> AuthService::currentUser(const std::string& token) {
    const auto userId = sessions_.resolve(token);
    return userId ? users_.findById(*userId) : std::nullopt;
}

void AuthService::logout(const std::string& token) {
    sessions_.remove(token);
}

}  // namespace ctd::gateway
