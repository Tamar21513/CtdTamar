#include "App/VisualApp.hpp"
#include "App/LobbyApp.hpp"
#include "Client/ClientApp.hpp"
#include "Network/ClientTransportConfig.hpp"

#include <charconv>
#include <exception>
#include <filesystem>
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
        const std::filesystem::path executablePath =
            std::filesystem::absolute(argv[0]);
        if (executablePath.has_parent_path()) {
            std::filesystem::current_path(
                executablePath.parent_path()
            );
        }

        enum class ClientMode { Lobby, Game, Text };
        ClientMode mode = ClientMode::Lobby;
        int endpointIndex = 1;
        if (
            argc >= 2 &&
            std::string(argv[1]) == "text"
        ) {
            mode = ClientMode::Text;
            endpointIndex = 2;
        }
        else if (
            argc >= 2 &&
            std::string(argv[1]) == "game"
        ) {
            mode = ClientMode::Game;
            endpointIndex = 2;
        }
        else if (
            argc >= 2 &&
            std::string(argv[1]) == "lobby"
        ) {
            mode = ClientMode::Lobby;
            endpointIndex = 2;
        }

        const int endpointArguments =
            argc - endpointIndex;
        if (
            endpointArguments != 0 &&
            endpointArguments != 2
        ) {
            std::cerr
                << "Usage: ctd_client.exe [lobby]\n"
                << "       ctd_client.exe lobby "
                   "[server-host port]\n"
                << "       ctd_client.exe game "
                   "[server-host port]\n"
                << "       ctd_client.exe text "
                   "[server-host port]\n";
            return 1;
        }

        if (mode == ClientMode::Lobby) {
            ctd::lobby::LobbyApp app;
            if (endpointArguments == 0) {
                app.run();
            }
            else {
                app.run(ctd::network::makeTransportConfig(
                    argv[endpointIndex],
                    parsePort(argv[endpointIndex + 1])
                ));
            }
        }
        else if (mode == ClientMode::Text) {
            ClientApp app;
            if (endpointArguments == 0) {
                app.run();
            }
            else {
                app.run(
                    argv[endpointIndex],
                    parsePort(argv[endpointIndex + 1])
                );
            }
        }
        else {
            VisualApp app;
            if (endpointArguments == 0) {
                app.run();
            }
            else {
                app.run(
                    argv[endpointIndex],
                    parsePort(argv[endpointIndex + 1])
                );
            }
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
