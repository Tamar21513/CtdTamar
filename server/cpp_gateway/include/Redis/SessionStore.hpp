#pragma once

#include <memory>
#include <optional>
#include <string>

namespace sw::redis { class Redis; }

namespace ctd::gateway {

class SessionStore {
public:
    SessionStore(std::string redisUri, int ttlSeconds);
    ~SessionStore();

    std::string create(const std::string& userId);
    std::optional<std::string> resolve(const std::string& token);
    void remove(const std::string& token);
    bool healthy() const;
    int ttl(const std::string& token) const;

private:
    static bool validToken(const std::string& token);
    static std::string key(const std::string& token);
    std::unique_ptr<sw::redis::Redis> redis_;
    int ttlSeconds_;
};

}  // namespace ctd::gateway
