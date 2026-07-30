#include "Redis/SessionStore.hpp"

#include <sodium.h>
#include <sw/redis++/redis++.h>
#include <boost/json.hpp>

#include <array>
#include <chrono>

namespace ctd::gateway {

SessionStore::SessionStore(std::string redisUri, int ttlSeconds)
    : redis_(std::make_unique<sw::redis::Redis>(redisUri)),
      ttlSeconds_(ttlSeconds) {}

SessionStore::~SessionStore() = default;

bool SessionStore::validToken(const std::string& token) {
    return token.size() >= 32 && token.size() <= 128 &&
        token.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
            "0123456789-_") == std::string::npos;
}

std::string SessionStore::key(const std::string& token) {
    return "ctd:session:" + token;
}

std::string SessionStore::create(const std::string& userId) {
    std::array<unsigned char, 32> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    std::array<char, sodium_base64_ENCODED_LEN(
        32, sodium_base64_VARIANT_URLSAFE_NO_PADDING)> encoded{};
    sodium_bin2base64(
        encoded.data(),
        encoded.size(),
        bytes.data(),
        bytes.size(),
        sodium_base64_VARIANT_URLSAFE_NO_PADDING);
    const std::string token(encoded.data());
    const auto payload = boost::json::serialize(boost::json::object{
        {"user_id", userId},
        {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
    });
    redis_->setex(
        key(token),
        std::chrono::seconds(ttlSeconds_),
        payload);
    return token;
}

std::optional<std::string> SessionStore::resolve(
    const std::string& token) {
    if (!validToken(token)) {
        return std::nullopt;
    }
    const auto value = redis_->get(key(token));
    if (!value) {
        return std::nullopt;
    }
    try {
        const auto payload =
            boost::json::parse(std::string(*value)).as_object();
        const auto* userId = payload.if_contains("user_id");
        if (!userId || !userId->is_string()) {
            return std::nullopt;
        }
        return std::string(userId->as_string());
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void SessionStore::remove(const std::string& token) {
    if (validToken(token)) {
        redis_->del(key(token));
    }
}

bool SessionStore::healthy() const {
    try {
        return redis_->ping() == "PONG";
    } catch (const std::exception&) {
        return false;
    }
}

int SessionStore::ttl(const std::string& token) const {
    if (!validToken(token)) {
        return -2;
    }
    return static_cast<int>(redis_->ttl(key(token)));
}

}  // namespace ctd::gateway
