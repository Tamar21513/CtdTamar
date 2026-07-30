#include "Auth/PasswordHasher.hpp"

#include <sodium.h>

#include <array>
#include <stdexcept>

namespace ctd::gateway {

PasswordHasher::PasswordHasher() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialization failed");
    }
}

std::string PasswordHasher::hash(const std::string& password) const {
    std::array<char, crypto_pwhash_STRBYTES> output{};
    if (crypto_pwhash_str(
            output.data(),
            password.data(),
            static_cast<unsigned long long>(password.size()),
            crypto_pwhash_OPSLIMIT_MODERATE,
            crypto_pwhash_MEMLIMIT_MODERATE) != 0) {
        throw std::runtime_error("password hashing failed");
    }
    return output.data();
}

bool PasswordHasher::verify(
    const std::string& passwordHash,
    const std::string& password) const {
    return crypto_pwhash_str_verify(
               passwordHash.c_str(),
               password.data(),
               static_cast<unsigned long long>(password.size())) == 0;
}

}  // namespace ctd::gateway
