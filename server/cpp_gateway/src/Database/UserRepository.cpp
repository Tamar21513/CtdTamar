#include "Database/UserRepository.hpp"

#include <sodium.h>

#include <array>

namespace ctd::gateway {
namespace {

std::string newUuid() {
    std::array<unsigned char, 16> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
    std::array<char, 37> value{};
    std::snprintf(
        value.data(),
        value.size(),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
        "%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5], bytes[6], bytes[7],
        bytes[8], bytes[9], bytes[10], bytes[11],
        bytes[12], bytes[13], bytes[14], bytes[15]);
    return value.data();
}

}  // namespace

UserRepository::UserRepository(std::string connectionString)
    : connectionString_(std::move(connectionString)) {}

User UserRepository::readUser(const pqxx::row& row) {
    return {
        row["id"].c_str(),
        row["username"].c_str(),
        row["password_hash"].c_str(),
        row["created_at"].c_str()};
}

User UserRepository::create(
    const std::string& username,
    const std::string& normalizedUsername,
    const std::string& passwordHash) {
    try {
        pqxx::connection connection(connectionString_);
        pqxx::work transaction(connection);
        const auto rows = transaction.exec_params(
            "INSERT INTO users "
            "(id, username, username_normalized, password_hash) "
            "VALUES ($1::uuid, $2, $3, $4) "
            "RETURNING id::text, username, password_hash, "
            "created_at::text",
            newUuid(),
            username,
            normalizedUsername,
            passwordHash);
        transaction.commit();
        return readUser(rows.one_row());
    } catch (const pqxx::unique_violation&) {
        throw DuplicateUsername();
    }
}

std::optional<User> UserRepository::findByUsername(
    const std::string& normalizedUsername) {
    pqxx::connection connection(connectionString_);
    pqxx::read_transaction transaction(connection);
    const auto rows = transaction.exec_params(
        "SELECT id::text, username, password_hash, created_at::text "
        "FROM users WHERE username_normalized=$1",
        normalizedUsername);
    if (rows.empty()) {
        return std::nullopt;
    }
    return readUser(rows.one_row());
}

std::optional<User> UserRepository::findById(const std::string& id) {
    pqxx::connection connection(connectionString_);
    pqxx::read_transaction transaction(connection);
    const auto rows = transaction.exec_params(
        "SELECT id::text, username, password_hash, created_at::text "
        "FROM users WHERE id=$1::uuid",
        id);
    if (rows.empty()) {
        return std::nullopt;
    }
    return readUser(rows.one_row());
}

bool UserRepository::healthy() const {
    try {
        pqxx::connection connection(connectionString_);
        pqxx::read_transaction transaction(connection);
        return transaction.exec("SELECT 1").one_row()[0].as<int>() == 1;
    } catch (const std::exception&) {
        return false;
    }
}

void UserRepository::migrate() {
    pqxx::connection connection(connectionString_);
    pqxx::work transaction(connection);
    transaction.exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "id UUID PRIMARY KEY,"
        "username VARCHAR(32) NOT NULL,"
        "username_normalized VARCHAR(32) NOT NULL,"
        "password_hash VARCHAR(512) NOT NULL,"
        "created_at TIMESTAMPTZ NOT NULL DEFAULT now(),"
        "updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),"
        "CONSTRAINT uq_users_username_normalized "
        "UNIQUE (username_normalized));"
        "CREATE UNIQUE INDEX IF NOT EXISTS "
        "ix_users_username_normalized "
        "ON users (username_normalized);");
    transaction.commit();
}

}  // namespace ctd::gateway
