// Windows socket definitions must be loaded first.
#include "../../include/Network/TcpConnection.hpp"

#include "../../include/Client/ClientApp.hpp"
#include "../../include/Client/GameClient.hpp"
#include "../../include/Client/ClientGameState.hpp"
#include "../../include/Client/NetworkController.hpp"

#include <iostream>
#include <stdexcept>

namespace {

// Initializes and cleans up Windows Sockets.
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

// Sends one request and waits for its matching response.
Message sendMove(
    GameClient& client,
    const Position& source,
    const Position& destination
) {
    Message request;

    request.type = MessageType::MoveRequest;
    request.sequence = client.createSequence();
    request.source = source;
    request.destination = destination;
    request.createdAtMs = 0;

    client.send(request);

    return client.waitForResponse(
        request.sequence,
        5000
    );
}

void printResponse(
    int number,
    const Message& response
) {
    std::cout
        << "Response "
        << number
        << ": accepted="
        << (response.accepted ? "true" : "false")
        << ", reason="
        << response.reason
        << '\n';
}

} // namespace

void ClientApp::run() {
    SocketEnvironment sockets;
    GameClient client;

    client.connectTo(
        "127.0.0.1",
        5050
    );

    std::cout
        << "Connected to game server\n";

    // Legal white-pawn move: e2 -> e3
    const Message firstResponse =
        sendMove(
            client,
            Position(6, 4),
            Position(5, 4)
        );

    printResponse(1, firstResponse);

    ClientGameState gameState;
    Message initialUpdate;

    if (!client.tryReceiveUpdate(initialUpdate)) {
        throw std::runtime_error(
            "Initial game-state update was not received"
        );
    }

    if (!gameState.applyMessage(initialUpdate)) {
        throw std::runtime_error(
            "Initial update has no snapshot"
        );
    }

    if (
        gameState.getSnapshot().pieces.size()
        != 32
    ) {
        throw std::runtime_error(
            "Initial snapshot must contain 32 pieces"
        );
    }

    std::cout
        << "Initial snapshot received: "
        << gameState.getSnapshot().pieces.size()
        << " pieces\n";

    if (!gameState.applyMessage(firstResponse)) {
        throw std::runtime_error(
            "Move response has no snapshot"
        );
    }

    if (!firstResponse.accepted) {
        throw std::runtime_error(
            "The legal move was rejected"
        );
    }

    // The source is empty and must be rejected.
    const Message secondResponse =
        sendMove(
            client,
            Position(4, 4),
            Position(3, 4)
        );

    printResponse(2, secondResponse);

    if (!gameState.applyMessage(secondResponse)) {
        throw std::runtime_error(
            "Rejected response has no snapshot"
        );
    }

    if (secondResponse.accepted) {
        throw std::runtime_error(
            "The move from an empty cell was accepted"
        );
    }

// Test board geometry:
// board starts at (0,0), every cell is 100x100.
NetworkController controller(
    client,
    gameState,
    0,
    0,
    100,
    100
);

// Select the black pawn at row 1, column 3.
const ControllerResult selectionResult =
    controller.click(
        350,
        150
    );

if (
    !selectionResult.handled ||
    !controller.hasSelection()
) {
    throw std::runtime_error(
        "NetworkController could not select the piece"
    );
}

// Request the legal move (1,3) -> (2,3).
const ControllerResult moveResult =
    controller.click(
        350,
        250
    );

    std::cout
        << "NetworkController move: accepted="
        << (moveResult.handled ? "true" : "false")
        << ", reason="
        << moveResult.reason
        << '\n';
    
    if (!moveResult.handled) {
        throw std::runtime_error(
            "NetworkController move was rejected: " +
            moveResult.reason
        );
    }
    
    if (controller.hasSelection()) {
        throw std::runtime_error(
            "Selection was not cleared after the response"
        );
    }
    
    std::cout
        << "NetworkController test passed\n";

    client.disconnect();

    std::cout
        << "Permanent GameClient test passed\n";
}