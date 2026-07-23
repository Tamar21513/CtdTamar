#include "../../include/Server/GameServer.hpp"
#include "../../include/Messaging/GameStateSnapshotBuilder.hpp"

#include "../../include/Core/Piece.hpp"
#include "../../include/Network/Protocol.hpp"

#include <chrono>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

GameServer::GameServer(unsigned short port)
    : port(port),
      engine(createInitialBoard()),
      messageBus(),
      messageHandler(engine, messageBus),
      tcpServer() {
}

// Creates a standard chess board.
Board GameServer::createInitialBoard() {
    Board board(8, 8);
    int nextId = 1;

    const PieceKind backRank[8] = {
        PieceKind::Rook,
        PieceKind::Knight,
        PieceKind::Bishop,
        PieceKind::Queen,
        PieceKind::King,
        PieceKind::Bishop,
        PieceKind::Knight,
        PieceKind::Rook
    };

    // Black back rank.
    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(0, col),
            std::make_shared<Piece>(
                nextId++,
                PieceColor::Black,
                backRank[col]
            )
        );
    }

    // Black pawns.
    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(1, col),
            std::make_shared<Piece>(
                nextId++,
                PieceColor::Black,
                PieceKind::Pawn
            )
        );
    }

    // White pawns.
    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(6, col),
            std::make_shared<Piece>(
                nextId++,
                PieceColor::White,
                PieceKind::Pawn
            )
        );
    }

    // White back rank.
    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(7, col),
            std::make_shared<Piece>(
                nextId++,
                PieceColor::White,
                backRank[col]
            )
        );
    }

    return board;
}

void GameServer::handleClient(
    TcpConnection& client
) {
    int processedRequests = 0;

    // Send the complete initial state.
    Message initialUpdate;

    initialUpdate.type =
        MessageType::GameStateUpdated;

    initialUpdate.sequence = 0;
    initialUpdate.accepted = true;
    initialUpdate.reason = "initial_state";

    initialUpdate.createdAtMs =
        engine.getCurrentTimeMs();

    initialUpdate.hasSnapshot = true;

    initialUpdate.snapshot =
        GameStateSnapshotBuilder::build(
            engine
        );

    client.sendMessage(
        Protocol::serialize(initialUpdate)
    );

    std::cout
        << "Initial game state sent\n";

    auto previousTime =
        std::chrono::steady_clock::now();

    auto lastStateUpdate =
        previousTime;

    constexpr long long
        POLL_INTERVAL_MS = 16;

    constexpr long long
        STATE_UPDATE_INTERVAL_MS = 50;

    while (true) {
        // Wait briefly for a request. The timeout
        // allows the game clock to keep advancing.
        const bool requestAvailable =
            client.waitForIncomingData(
                POLL_INTERVAL_MS
            );

        const auto currentTime =
            std::chrono::steady_clock::now();

        const long long deltaMs =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                currentTime - previousTime
            ).count();

        previousTime = currentTime;

        // RealTimeArbiter remains responsible
        // for producing all timed game events.
        if (deltaMs > 0) {
            engine.wait(deltaMs);
        }

        if (requestAvailable) {
            const std::string requestJson =
                client.receiveMessage();

            if (requestJson.empty()) {
                std::cout
                    << "Client disconnected\n";
                break;
            }

            const Message request =
                Protocol::deserialize(
                    requestJson
                );

            std::cout
                << "Request received. Sequence: "
                << request.sequence
                << '\n';

            messageBus.sendToEngine(request);

            if (
                !messageHandler.processNextMessage()
            ) {
                throw std::runtime_error(
                    "Server could not process the request"
                );
            }

            if (
                !messageBus.hasMessageForClient()
            ) {
                throw std::runtime_error(
                    "GameEngine did not produce a response"
                );
            }

            const Message response =
                messageBus.receiveForClient();

            client.sendMessage(
                Protocol::serialize(response)
            );

            ++processedRequests;

            std::cout
                << "Response sent. Accepted: "
                << (
                    response.accepted
                        ? "true"
                        : "false"
                )
                << ", reason: "
                << response.reason
                << '\n';
        }

        const long long timeSinceUpdateMs =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                currentTime - lastStateUpdate
            ).count();

        // Send the authoritative state roughly
        // twenty times per second.
        if (
            timeSinceUpdateMs >=
            STATE_UPDATE_INTERVAL_MS
        ) {
            Message stateUpdate;

            stateUpdate.type =
                MessageType::GameStateUpdated;

            stateUpdate.sequence = 0;
            stateUpdate.accepted = true;
            stateUpdate.reason = "state_update";

            stateUpdate.createdAtMs =
                engine.getCurrentTimeMs();

            stateUpdate.hasSnapshot = true;

            stateUpdate.snapshot =
                GameStateSnapshotBuilder::build(
                    engine
                );

            client.sendMessage(
                Protocol::serialize(stateUpdate)
            );

            lastStateUpdate = currentTime;
        }
    }

    std::cout
        << "Processed requests from client: "
        << processedRequests
        << '\n';
}

// Accepts clients continuously.
// Clients are handled one after another at this stage.
void GameServer::run() {
    tcpServer.start(port);

    std::cout
        << "Game server listening on port "
        << port
        << "...\n";

    while (true) {
        std::cout << "Waiting for client...\n";

        try {
            TcpConnection client =
                tcpServer.acceptClient();

            std::cout << "Client connected\n";

            handleClient(client);
        }
        catch (const std::exception& exception) {
            // A client error must not terminate the server.
            messageBus.clear();

            std::cerr
                << "Client error: "
                << exception.what()
                << '\n';
        }
    }
}