#pragma once

#include <string>

namespace ctd::gateway {

class PasswordHasher {
public:
    PasswordHasher();
    std::string hash(const std::string& password) const;
    bool verify(
        const std::string& passwordHash,
        const std::string& password) const;
};

}  // namespace ctd::gateway
