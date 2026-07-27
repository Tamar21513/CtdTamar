#include "../include/Network/TcpConnection.hpp"
#include "../include/Network/Protocol.hpp"
#include "../include/Server/GameServer.hpp"

#include <iostream>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

namespace {

constexpr unsigned short TEST_PORT = 5051;

Message receiveUntil(
    TcpConnection& connection,
    MessageType type
) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        const std::string json =
            connection.receiveMessage();
        if (json.empty()) {
            throw std::runtime_error(
                "Connection closed before expected message"
            );
        }

        const Message message =
            Protocol::deserialize(json);
        if (message.type == type) return message;
    }

    throw std::runtime_error(
        "Expected message was not received"
    );
}

void sendHandshake(
    TcpConnection& connection,
    const std::string& username,
    const std::string& token = ""
) {
    Message handshake;
    handshake.type = MessageType::ReconnectRequest;
    handshake.username = username;
    handshake.reconnectToken = token;
    connection.sendMessage(
        Protocol::serialize(handshake)
    );
}

void runClient() {
    TcpConnection invalid;
    invalid.connectTo("127.0.0.1", TEST_PORT);
    sendHandshake(invalid, "   ");
    const Message invalidResponse =
        receiveUntil(
            invalid,
            MessageType::InvalidUsername
        );
    if (invalidResponse.reason != "empty_username") {
        throw std::runtime_error(
            "Whitespace username was not rejected"
        );
    }

    TcpConnection white;
    white.connectTo("127.0.0.1", TEST_PORT);
    sendHandshake(white, "Alice Smith");
    const Message whiteAssignment =
        receiveUntil(
            white,
            MessageType::PlayerAssigned
        );
    if (
        whiteAssignment.playerColor != "white" ||
        whiteAssignment.username != "Alice Smith"
    ) {
        throw std::runtime_error(
            "First player identity is incorrect"
        );
    }
    receiveUntil(
        white,
        MessageType::WaitingForOpponent
    );

    TcpConnection black;
    black.connectTo("127.0.0.1", TEST_PORT);
    sendHandshake(black, "Bob Jones");
    const Message blackAssignment =
        receiveUntil(
            black,
            MessageType::PlayerAssigned
        );
    if (
        blackAssignment.playerColor != "black" ||
        blackAssignment.username != "Bob Jones"
    ) {
        throw std::runtime_error(
            "Second player identity is incorrect"
        );
    }

    const Message whiteStarted =
        receiveUntil(white, MessageType::GameStarted);
    const Message blackStarted =
        receiveUntil(black, MessageType::GameStarted);
    if (
        whiteStarted.whiteUsername != "Alice Smith" ||
        whiteStarted.blackUsername != "Bob Jones" ||
        blackStarted.whiteUsername != "Alice Smith" ||
        blackStarted.blackUsername != "Bob Jones"
    ) {
        throw std::runtime_error(
            "Clients did not receive both usernames"
        );
    }

    Message earlyMove;
    earlyMove.type = MessageType::MoveRequest;
    earlyMove.sequence = 41;
    earlyMove.source = Position(6, 0);
    earlyMove.destination = Position(5, 0);
    white.sendMessage(Protocol::serialize(earlyMove));
    const Message earlyRejection =
        receiveUntil(white, MessageType::MoveRejected);
    if (
        earlyRejection.reason !=
        "countdown_in_progress"
    ) {
        throw std::runtime_error(
            "Move was not rejected before GO"
        );
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(4200)
    );

    TcpConnection third;
    third.connectTo("127.0.0.1", TEST_PORT);
    sendHandshake(third, "Charlie");
    receiveUntil(third, MessageType::GameFull);

    Message wrongColorMove;
    wrongColorMove.type = MessageType::MoveRequest;
    wrongColorMove.sequence = 42;
    wrongColorMove.source = Position(1, 0);
    wrongColorMove.destination = Position(2, 0);
    white.sendMessage(
        Protocol::serialize(wrongColorMove)
    );
    const Message rejected =
        receiveUntil(white, MessageType::MoveRejected);
    if (rejected.reason != "not_your_piece") {
        throw std::runtime_error(
            "Opponent piece move was not rejected"
        );
    }

    const std::string blackToken =
        blackAssignment.reconnectToken;
    black.close();
    receiveUntil(white, MessageType::Reconnecting);

    TcpConnection restored;
    restored.connectTo("127.0.0.1", TEST_PORT);
    sendHandshake(restored, "Mallory", blackToken);
    const Message restoredAssignment =
        receiveUntil(
            restored,
            MessageType::PlayerAssigned
        );
    if (
        restoredAssignment.playerColor != "black" ||
        restoredAssignment.username != "Bob Jones"
    ) {
        throw std::runtime_error(
            "Reconnect changed the player identity"
        );
    }

    std::cout << "Username server smoke test passed\n";
}

} // namespace

int main(int argc, char* argv[]) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return 1;
    }

    try {
        if (argc == 2 && std::string(argv[1]) == "server") {
            GameServer server(TEST_PORT);
            server.run();
        }
        else {
            runClient();
        }
    }
    catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}
