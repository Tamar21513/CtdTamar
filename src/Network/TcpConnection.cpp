#include "../../include/Network/TcpConnection.hpp"

#include <stdexcept>
#include <utility>

// Creates an empty TCP connection.
TcpConnection::TcpConnection()
    : socketHandle(INVALID_SOCKET) {}

// Stores an already connected socket.
TcpConnection::TcpConnection(SOCKET socketHandle)
    : socketHandle(socketHandle) {}

// Transfers socket ownership.
TcpConnection::TcpConnection(
    TcpConnection&& other
) noexcept
    : socketHandle(other.socketHandle) {
    other.socketHandle = INVALID_SOCKET;
}

// Transfers socket ownership and closes the previous socket.
TcpConnection& TcpConnection::operator=(
    TcpConnection&& other
) noexcept {
    if (this != &other) {
        close();

        socketHandle = other.socketHandle;
        other.socketHandle = INVALID_SOCKET;
    }

    return *this;
}

// Closes the socket when the object is destroyed.
TcpConnection::~TcpConnection() {
    close();
}

// Connects to a TCP server.
void TcpConnection::connectTo(
    const std::string& ip,
    unsigned short port
) {
    close();

    socketHandle = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (socketHandle == INVALID_SOCKET) {
        throw std::runtime_error(
            "TcpConnection: socket creation failed"
        );
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (
        inet_pton(
            AF_INET,
            ip.c_str(),
            &address.sin_addr
        ) != 1
    ) {
        close();

        throw std::runtime_error(
            "TcpConnection: invalid server IP"
        );
    }

    if (
        connect(
            socketHandle,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)
        ) == SOCKET_ERROR
    ) {
        close();

        throw std::runtime_error(
            "TcpConnection: connection failed"
        );
    }
}

// Sends every byte of the supplied data.
void TcpConnection::sendAll(
    const std::string& data
) {
    std::size_t sentBytes = 0;

    while (sentBytes < data.size()) {
        const int result = send(
            socketHandle,
            data.data() + sentBytes,
            static_cast<int>(
                data.size() - sentBytes
            ),
            0
        );

        if (result == SOCKET_ERROR || result == 0) {
            throw std::runtime_error(
                "TcpConnection: send failed"
            );
        }

        sentBytes +=
            static_cast<std::size_t>(result);
    }
}

// Sends one newline-terminated JSON message.
void TcpConnection::sendMessage(
    const std::string& json
) {
    if (!isOpen()) {
        throw std::runtime_error(
            "TcpConnection: socket is not open"
        );
    }

    sendAll(json + "\n");
}

// Receives one complete message.
// Returns an empty string when the other side closes the connection.
std::string TcpConnection::receiveMessage() {
    if (!isOpen()) {
        throw std::runtime_error(
            "TcpConnection: socket is not open"
        );
    }

    std::string message;
    char currentCharacter = '\0';

    while (true) {
        const int result = recv(
            socketHandle,
            &currentCharacter,
            1,
            0
        );

        // The other side closed the connection normally.
        if (result == 0) {
            return "";
        }

        if (result == SOCKET_ERROR) {
            throw std::runtime_error(
                "TcpConnection: receive failed"
            );
        }

        if (currentCharacter == '\n') {
            break;
        }

        message.push_back(currentCharacter);
    }

    return message;
}

// Waits for socket data without permanently
// blocking the server game loop.
bool TcpConnection::waitForIncomingData(
    long long timeoutMs
) const {
    if (!isOpen()) {
        throw std::runtime_error(
            "TcpConnection: socket is not open"
        );
    }

    if (timeoutMs < 0) {
        throw std::invalid_argument(
            "TcpConnection: timeout cannot be negative"
        );
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socketHandle, &readSet);

    timeval timeout;
    timeout.tv_sec =
        static_cast<long>(timeoutMs / 1000);

    timeout.tv_usec =
        static_cast<long>(
            (timeoutMs % 1000) * 1000
        );

    // On Windows, the first argument of select
    // is ignored.
    const int result = select(
        0,
        &readSet,
        nullptr,
        nullptr,
        &timeout
    );

    if (result == SOCKET_ERROR) {
        throw std::runtime_error(
            "TcpConnection: select failed"
        );
    }

    return
        result > 0 &&
        FD_ISSET(socketHandle, &readSet);
}

// Reports whether a socket is currently owned.
bool TcpConnection::isOpen() const {
    return socketHandle != INVALID_SOCKET;
}

// Closes the current socket.
void TcpConnection::close() {
    if (socketHandle != INVALID_SOCKET) {
        shutdown(socketHandle, SD_BOTH);
        closesocket(socketHandle);
        socketHandle = INVALID_SOCKET;
    }
}