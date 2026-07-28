#ifndef USERNAME_HPP
#define USERNAME_HPP

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Username {

constexpr std::size_t MAX_LENGTH = 64;

inline std::string trim(const std::string& value) {
    std::size_t first = 0;
    while (
        first < value.size() &&
        std::isspace(
            static_cast<unsigned char>(value[first])
        )
    ) {
        ++first;
    }

    std::size_t last = value.size();
    while (
        last > first &&
        std::isspace(
            static_cast<unsigned char>(value[last - 1])
        )
    ) {
        --last;
    }

    return value.substr(first, last - first);
}

inline bool isValid(const std::string& value) {
    const std::string trimmed = trim(value);
    return
        !trimmed.empty() &&
        trimmed.size() <= MAX_LENGTH;
}

inline std::string prompt(
    std::istream& input,
    std::ostream& output
) {
    std::string value;
    while (true) {
        output << "Enter display name: ";
        if (!std::getline(input, value)) {
            throw std::runtime_error(
                "Unable to read display name"
            );
        }

        value = trim(value);
        if (isValid(value)) return value;

        output
            << "Display name must contain 1-"
            << MAX_LENGTH
            << " non-whitespace bytes.\n";
    }
}

} // namespace Username

#endif
