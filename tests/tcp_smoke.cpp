#include "../include/Network/TcpConnection.hpp"
#include "../include/Network/TcpServer.hpp"
#include "../include/Network/Protocol.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

// Initializes the Windows socket system.
void initializeSockets() {
    WSADATA data{};

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw std::runtime_error(
            "WSAStartup failed"
        );
    }
}

// Runs the server side of the network test.
void runServer() {
    TcpServer server;
    server.start(5050);

    std::cout << "Server waiting on port 5050...\n";

    TcpConnection client =
        server.acceptClient();

    std::cout << "Client connected\n";

    const std::string json =
        client.receiveMessage();

    const Message request =
        Protocol::deserialize(json);

    std::cout
        << "Received sequence: "
        << request.sequence
        << '\n';

    request.type; // The received request remains unchanged.
    client.sendMessage(
        Protocol::serialize(request)
    );

    std::cout << "Response sent\n";
}

// Runs the client side of the network test.
void runClient() {
    TcpConnection connection;
    connection.connectTo("127.0.0.1", 5050);

    Message request;
    request.type = MessageType::MoveRequest;
    request.sequence = 100;
    request.source = Position(6, 4);
    request.destination = Position(5, 4);
    request.createdAtMs = 0;

    connection.sendMessage(
        Protocol::serialize(request)
    );

    const Message response =
        Protocol::deserialize(
            connection.receiveMessage()
        );

    std::cout
        << "Response sequence: "
        << response.sequence
        << '\n';

    if (response.sequence != request.sequence) {
        throw std::runtime_error(
            "Unexpected response sequence"
        );
    }

    std::cout << "TCP test passed\n";
}

// Selects the server or client test mode.
int main(int argc, char* argv[]) {
    try {
        initializeSockets();

        if (argc < 2) {
            std::cout
                << "Usage: tcp_smoke.exe server|client\n";

            WSACleanup();
            return 1;
        }

        const std::string mode = argv[1];

        if (mode == "server") {
            runServer();
        }
        else if (mode == "client") {
            runClient();
        }
        else {
            throw std::runtime_error(
                "Unknown test mode"
            );
        }

        WSACleanup();
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr
            << "Error: "
            << exception.what()
            << '\n';

        WSACleanup();
        return 1;
    }
}