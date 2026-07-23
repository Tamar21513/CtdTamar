#include "../../include/Network/TcpServer.hpp"

#include <stdexcept>

// Creates an inactive server.
TcpServer::TcpServer()
    : listeningSocket(INVALID_SOCKET) {}

// Stops the server when the object is destroyed.
TcpServer::~TcpServer() {
    stop();
}

// Opens a listening socket on the supplied port.
void TcpServer::start(unsigned short port) {
    stop();

    listeningSocket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (listeningSocket == INVALID_SOCKET) {
        throw std::runtime_error(
            "TcpServer: socket creation failed"
        );
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (
        bind(
            listeningSocket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)
        ) == SOCKET_ERROR
    ) {
        stop();

        throw std::runtime_error(
            "TcpServer: bind failed"
        );
    }

    if (listen(listeningSocket, SOMAXCONN) == SOCKET_ERROR) {
        stop();

        throw std::runtime_error(
            "TcpServer: listen failed"
        );
    }
}

// Waits for and returns one connected client.
TcpConnection TcpServer::acceptClient() {
    const SOCKET clientSocket = accept(
        listeningSocket,
        nullptr,
        nullptr
    );

    if (clientSocket == INVALID_SOCKET) {
        throw std::runtime_error(
            "TcpServer: accept failed"
        );
    }

    return TcpConnection(clientSocket);
}

// Closes the listening socket.
void TcpServer::stop() {
    if (listeningSocket != INVALID_SOCKET) {
        closesocket(listeningSocket);
        listeningSocket = INVALID_SOCKET;
    }
}