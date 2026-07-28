// Load Windows socket definitions before the project headers.
#include "Network/TcpConnection.hpp"

#include "Server/ServerApp.hpp"
#include "Server/GameServer.hpp"
#include "Network/NetworkDefaults.hpp"

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
    run(NetworkDefaults::PORT);
}

// Runs the server on the requested TCP port.
void ServerApp::run(unsigned short port) {
    SocketEnvironment sockets;

    GameServer server(port);
    server.run();
}
