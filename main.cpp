#include "include/App/ConsoleApp.hpp"
#include "include/App/VisualApp.hpp"
#include "include/Client/ClientApp.hpp"
#include "include/Server/ServerApp.hpp"
#include <charconv>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

// Parses and validates one TCP port argument.
unsigned short parsePort(const std::string& text) {
    unsigned long long value = 0;

    const auto result =
        std::from_chars(
            text.data(),
            text.data() + text.size(),
            value
        );

    if (
        text.empty() ||
        result.ec ==
            std::errc::invalid_argument ||
        result.ptr != text.data() + text.size()
    ) {
        throw std::invalid_argument(
            "Invalid port: " + text
        );
    }

    if (
        result.ec ==
            std::errc::result_out_of_range ||
        value < 1 ||
        value > 65535
    ) {
        throw std::out_of_range(
            "Port must be between 1 and 65535"
        );
    }

    return static_cast<unsigned short>(value);
}

// Prints every supported command-line form.
void printUsage() {
    std::cout << "Usage:\n";
    std::cout << "  main.exe console\n";
    std::cout << "  main.exe server [port]\n";
    std::cout
        << "  main.exe visual [server-host port]\n";
    std::cout
        << "  main.exe client [server-host port]\n";
}

} // namespace

// Selects and starts the requested application mode.
int main(int argc, char* argv[]) {
    try {
        if (argc >= 2) {
            const std::string mode = argv[1];

            if (mode == "console") {
                ConsoleApp app;
                app.run();
                return 0;
            }

            if (mode == "visual") {
                VisualApp app;

                if (argc == 2) {
                    app.run();
                }
                else if (argc == 4) {
                    app.run(
                        argv[2],
                        parsePort(argv[3])
                    );
                }
                else {
                    printUsage();
                    return 1;
                }

                return 0;
            }

            if (mode == "server") {
                ServerApp app;

                if (argc == 2) {
                    app.run();
                }
                else if (argc == 3) {
                    app.run(
                        parsePort(argv[2])
                    );
                }
                else {
                    printUsage();
                    return 1;
                }

                return 0;
            }

            if (mode == "client") {
                ClientApp app;

                if (argc == 2) {
                    app.run();
                }
                else if (argc == 4) {
                    app.run(
                        argv[2],
                        parsePort(argv[3])
                    );
                }
                else {
                    printUsage();
                    return 1;
                }

                return 0;
            }
        }

        printUsage();

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
