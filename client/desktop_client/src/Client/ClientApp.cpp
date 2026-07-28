// Windows socket definitions must be loaded first.
#include "Network/TcpConnection.hpp"

#include "Client/ClientApp.hpp"
#include "Client/GameClient.hpp"
#include "Client/GameStartState.hpp"
#include "Client/Username.hpp"
#include "Client/ClientGameState.hpp"
#include "Client/NetworkController.hpp"
#include "Core/Piece.hpp"
#include "Network/NetworkDefaults.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

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

// Waits for an assignment, snapshot, and game start.
void waitForReadyGame(
    GameClient& client,
    ClientGameState& gameState
) {
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(15);

    while (
        std::chrono::steady_clock::now() <
        deadline
    ) {
        Message update;

        while (client.tryReceiveUpdate(update)) {
            gameState.applyMessage(update);
        }

        const GameStateSnapshot snapshot =
            gameState.copySnapshot();

        if (
            client.hasPlayerAssignment() &&
            client.hasGameStarted() &&
            gameState.hasSnapshot() &&
            buildGameStartDisplay(
                snapshot.playersReady,
                snapshot.gameplayStartsAtEpochMs,
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(
                    std::chrono::system_clock::now()
                        .time_since_epoch()
                ).count()
            ).phase ==
                ClientUiPhase::Playing
        ) {
            return;
        }

        if (!client.isConnected()) {
            throw std::runtime_error(
                "Connection lost while waiting "
                "for the game to start"
            );
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10)
        );
    }

    throw std::runtime_error(
        "Timed out while waiting for the game to start"
    );
}

} // namespace

// Runs the client with the default endpoint.
void ClientApp::run() {
    run(
        NetworkDefaults::HOST,
        NetworkDefaults::PORT
    );
}

// Runs the client against one server endpoint.
void ClientApp::run(
    const std::string& serverHost,
    unsigned short serverPort
) {
    SocketEnvironment sockets;
    GameClient client;
    const std::string username =
        Username::prompt(std::cin, std::cout);

    try {
        client.connectTo(
            serverHost,
            serverPort,
            username
        );
    }
    catch (...) {
        throw std::runtime_error(
            "Unable to connect to " +
            serverHost +
            ":" +
            std::to_string(serverPort)
        );
    }

    std::cout
        << "Connected to game server\n";

    ClientGameState gameState;
    waitForReadyGame(client, gameState);

    std::cout
        << "Assigned player: "
        << client.getUsername()
        << " ("
        << (
            client.getAssignedColor() ==
                    PieceColor::White
                ? "white"
                : "black"
        )
        << ")\n";

    const bool isWhite =
        client.getAssignedColor() ==
        PieceColor::White;

    const Position firstSource =
        isWhite
            ? Position(6, 4)
            : Position(1, 4);

    const Position firstDestination =
        isWhite
            ? Position(5, 4)
            : Position(2, 4);

    // Sends one legal pawn move for the assigned color.
    const Message firstResponse =
        sendMove(
            client,
            firstSource,
            firstDestination
        );

    printResponse(1, firstResponse);

    if (
        gameState.copySnapshot().pieces.size()
        != 32
    ) {
        throw std::runtime_error(
            "Initial snapshot must contain 32 pieces"
        );
    }

    std::cout
        << "Initial snapshot received: "
        << gameState.copySnapshot().pieces.size()
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

const int ownPawnRow =
    isWhite ? 6 : 1;

const int ownDestinationRow =
    isWhite ? 5 : 2;

// Select a pawn belonging to this client.
const ControllerResult selectionResult =
    controller.click(
        350,
        ownPawnRow * 100 + 50
    );

if (
    !selectionResult.handled ||
    !controller.hasSelection()
) {
    throw std::runtime_error(
        "NetworkController could not select the piece"
    );
}

// Request one legal move for the selected pawn.
const ControllerResult moveResult =
    controller.click(
        350,
        ownDestinationRow * 100 + 50
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
