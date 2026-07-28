#ifndef TCP_CONNECTION_HPP
#define TCP_CONNECTION_HPP

#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

class TcpConnection {
private:
    SOCKET socketHandle;

    // Sends all bytes even when send transmits only part of the message.
    void sendAll(const std::string& data);

public:
    // Creates an empty TCP connection.
    TcpConnection();

    // Creates a TCP connection from an existing socket.
    explicit TcpConnection(SOCKET socketHandle);

    // Prevents accidental copying of a socket.
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Transfers ownership of another socket.
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    // Closes the socket.
    ~TcpConnection();

    // Connects this socket to a server.
    void connectTo(
        const std::string& ip,
        unsigned short port
    );

    // Sends one complete JSON message.
    void sendMessage(const std::string& json);

    // Receives one complete JSON message.
    std::string receiveMessage();

    // Waits until data is available or the timeout expires.
    bool waitForIncomingData(
        long long timeoutMs
    ) const;

    // Reports whether the socket is open.
    bool isOpen() const;

    // Closes the socket.
    void close();
};

#endif