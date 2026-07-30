#pragma once

#include <string>

namespace ctd::gateway {

struct User {
    std::string id;
    std::string username;
    std::string passwordHash;
    std::string createdAt;
};

}  // namespace ctd::gateway
