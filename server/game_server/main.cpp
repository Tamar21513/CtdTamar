#include "App/ConsoleApp.hpp"
#include "Server/ServerApp.hpp"

#include <charconv>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

unsigned short parsePort(const std::string& text) {
    unsigned long long value = 0;
    const auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value
    );

    if (
        text.empty() ||
        result.ec != std::errc() ||
        result.ptr != text.data() + text.size() ||
        value < 1 ||
        value > 65535
    ) {
        throw std::invalid_argument(
            "Port must be between 1 and 65535"
        );
    }

    return static_cast<unsigned short>(value);
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (
            argc >= 2 &&
            std::string(argv[1]) == "console"
        ) {
            if (argc != 2) {
                std::cerr
                    << "Usage: ctd_server.exe console\n";
                return 1;
            }
            ConsoleApp app;
            app.run();
            return 0;
        }

        ServerApp app;
        if (argc == 1) {
            app.run();
        }
        else if (argc == 2) {
            app.run(parsePort(argv[1]));
        }
        else {
            std::cerr
                << "Usage: ctd_server.exe [port]\n"
                << "       ctd_server.exe console\n";
            return 1;
        }
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr
            << "Error: "
            << exception.what()
            << '\n';
        return 1;
    }
}
