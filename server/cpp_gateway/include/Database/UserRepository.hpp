#pragma once

#include "Auth/User.hpp"

#include <optional>
#include <pqxx/pqxx>
#include <string>

namespace ctd::gateway {

class DuplicateUsername final : public std::runtime_error {
public:
    DuplicateUsername() : std::runtime_error("duplicate username") {}
};

class UserRepository {
public:
    explicit UserRepository(std::string connectionString);

    User create(
        const std::string& username,
        const std::string& normalizedUsername,
        const std::string& passwordHash);
    std::optional<User> findByUsername(
        const std::string& normalizedUsername);
    std::optional<User> findById(const std::string& id);
    bool healthy() const;
    void migrate();

private:
    static User readUser(const pqxx::row& row);
    std::string connectionString_;
};

}  // namespace ctd::gateway
