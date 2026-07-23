#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include "TcpConnection.hpp"

class TcpServer {
private:
    SOCKET listeningSocket;

public:
    // Creates an inactive TCP server.
    TcpServer();

    // Prevents copying the listening socket.
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // Closes the listening socket.
    ~TcpServer();

    // Starts listening for client connections.
    void start(unsigned short port);

    // Waits until one client connects.
    TcpConnection acceptClient();

    // Closes the listening socket.
    void stop();
};

#endif