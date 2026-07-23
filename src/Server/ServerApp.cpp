// Load Windows socket definitions before the project headers.
#include "../../include/Network/TcpConnection.hpp"

#include "../../include/Server/ServerApp.hpp"
#include "../../include/Server/GameServer.hpp"

#include <stdexcept>

namespace {
    class SocketEnvironment {
    public:
        SocketEnvironment() {
            WSADATA data{};

            const int result =
                WSAStartup(MAKEWORD(2, 2), &data);

            if (result != 0) {
                throw std::runtime_error(
                    "WSAStartup failed"
                );
            }
        }

        ~SocketEnvironment() {
            WSACleanup();
        }

        SocketEnvironment(
            const SocketEnvironment&
        ) = delete;

        SocketEnvironment& operator=(
            const SocketEnvironment&
        ) = delete;
    };
}

void ServerApp::run() {
    SocketEnvironment sockets;

    GameServer server(5050);
    server.run();
}