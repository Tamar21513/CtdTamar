#pragma once

#include "Auth/PasswordHasher.hpp"
#include "Auth/User.hpp"
#include "Database/UserRepository.hpp"
#include "Redis/SessionStore.hpp"

#include <optional>
#include <string>

namespace ctd::gateway {

struct LoginResult {
    User user;
    std::string sessionToken;
};

class AuthService {
public:
    AuthService(
        UserRepository& users,
        SessionStore& sessions,
        PasswordHasher& passwords);

    User registerUser(
        const std::string& username,
        const std::string& password);
    std::optional<LoginResult> login(
        const std::string& username,
        const std::string& password);
    std::optional<User> currentUser(const std::string& token);
    void logout(const std::string& token);

    static std::string validateUsername(const std::string& username);
    static bool validPassword(const std::string& password);
    static std::string normalizeUsername(const std::string& username);

private:
    UserRepository& users_;
    SessionStore& sessions_;
    PasswordHasher& passwords_;
};

}  // namespace ctd::gateway
