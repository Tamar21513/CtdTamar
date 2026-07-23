#include "../include/Network/TcpConnection.hpp"
#include "../include/Network/TcpServer.hpp"

#include "../include/Core/Board.hpp"
#include "../include/Core/Piece.hpp"
#include "../include/Engine/GameEngine.hpp"
#include "../include/Messaging/EngineMessageHandler.hpp"
#include "../include/Messaging/MessageBus.hpp"
#include "../include/Network/Protocol.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

// Initializes Windows Sockets.
void initializeSockets() {
    WSADATA data{};

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw runtime_error("WSAStartup failed");
    }
}

// Creates the standard initial chess board.
Board createInitialBoard() {
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

    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(0, col),
            make_shared<Piece>(
                nextId++,
                PieceColor::Black,
                backRank[col]
            )
        );
    }

    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(1, col),
            make_shared<Piece>(
                nextId++,
                PieceColor::Black,
                PieceKind::Pawn
            )
        );
    }

    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(6, col),
            make_shared<Piece>(
                nextId++,
                PieceColor::White,
                PieceKind::Pawn
            )
        );
    }

    for (int col = 0; col < 8; ++col) {
        board.placePiece(
            Position(7, col),
            make_shared<Piece>(
                nextId++,
                PieceColor::White,
                backRank[col]
            )
        );
    }

    return board;
}

// Runs a game server that handles multiple requests
// through the same TCP connection.
void runServer() {
    GameEngine engine(createInitialBoard());

    MessageBus messageBus;

    EngineMessageHandler messageHandler(
        engine,
        messageBus
    );

    TcpServer server;
    server.start(5050);

    cout << "Game server waiting on port 5050...\n";

    TcpConnection client =
        server.acceptClient();

    cout << "Client connected\n";

    int processedRequests = 0;

    while (true) {
        const string requestJson =
            client.receiveMessage();

        // An empty result means the client disconnected.
        if (requestJson.empty()) {
            cout << "Client disconnected\n";
            break;
        }

        const Message request =
            Protocol::deserialize(requestJson);

        cout
            << "Request received. Sequence: "
            << request.sequence
            << '\n';

        messageBus.sendToEngine(request);

        if (!messageHandler.processNextMessage()) {
            throw runtime_error(
                "The server did not process the request"
            );
        }

        if (!messageBus.hasMessageForClient()) {
            throw runtime_error(
                "GameEngine did not produce a response"
            );
        }

        const Message response =
            messageBus.receiveForClient();

        client.sendMessage(
            Protocol::serialize(response)
        );

        ++processedRequests;

        cout
            << "Response sent. Accepted: "
            << (response.accepted ? "true" : "false")
            << ", reason: "
            << response.reason
            << '\n';
    }

    cout
        << "Processed requests: "
        << processedRequests
        << '\n';
}

// Sends one request and waits for its matching response.
Message sendRequestAndReceiveResponse(
    TcpConnection& connection,
    const Message& request
) {
    connection.sendMessage(
        Protocol::serialize(request)
    );

    const string responseJson =
        connection.receiveMessage();

    if (responseJson.empty()) {
        throw runtime_error(
            "Server disconnected before sending a response"
        );
    }

    const Message response =
        Protocol::deserialize(responseJson);

    if (response.sequence != request.sequence) {
        throw runtime_error(
            "Unexpected response sequence"
        );
    }

    return response;
}

// Sends several game requests through one TCP connection.
void runClient() {
    TcpConnection connection;

    connection.connectTo(
        "127.0.0.1",
        5050
    );

    cout << "Connected to game server\n";

    Message firstRequest;

    firstRequest.type =
        MessageType::MoveRequest;

    firstRequest.sequence = 300;

    firstRequest.source =
        Position(6, 4);

    firstRequest.destination =
        Position(5, 4);

    firstRequest.createdAtMs = 0;

    const Message firstResponse =
        sendRequestAndReceiveResponse(
            connection,
            firstRequest
        );

    cout
        << "Response 1: accepted="
        << (firstResponse.accepted ? "true" : "false")
        << ", reason="
        << firstResponse.reason
        << '\n';

    if (!firstResponse.accepted) {
        throw runtime_error(
            "The first legal move was rejected"
        );
    }

    // Send another request without reconnecting.
    // This request uses an empty source cell and should be rejected.
    Message secondRequest;

    secondRequest.type =
        MessageType::MoveRequest;

    secondRequest.sequence = 301;

    secondRequest.source =
        Position(4, 4);

    secondRequest.destination =
        Position(3, 4);

    secondRequest.createdAtMs = 0;

    const Message secondResponse =
        sendRequestAndReceiveResponse(
            connection,
            secondRequest
        );

    cout
        << "Response 2: accepted="
        << (secondResponse.accepted ? "true" : "false")
        << ", reason="
        << secondResponse.reason
        << '\n';

    if (secondResponse.accepted) {
        throw runtime_error(
            "The move from an empty cell was accepted"
        );
    }

    connection.close();

    cout << "Persistent game server test passed\n";
}

// Selects server or client mode.
int main(int argc, char* argv[]) {
    try {
        initializeSockets();

        if (argc < 2) {
            cout
                << "Usage: game_server_smoke.exe "
                << "server|client\n";

            WSACleanup();
            return 1;
        }

        const string mode = argv[1];

        if (mode == "server") {
            runServer();
        }
        else if (mode == "client") {
            runClient();
        }
        else {
            throw runtime_error(
                "Unknown mode"
            );
        }

        WSACleanup();
        return 0;
    }
    catch (const exception& exception) {
        cerr
            << "Error: "
            << exception.what()
            << '\n';

        WSACleanup();
        return 1;
    }
}