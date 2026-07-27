#include "../../include/Server/GameServer.hpp"

#include "../../include/Core/Piece.hpp"
#include "../../include/Messaging/GameStateSnapshotBuilder.hpp"
#include "../../include/Network/Protocol.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Creates the server and its authoritative game state.
GameServer::GameServer(unsigned short port)
    : port(port),
      engine(createInitialBoard()),
      messageBus(),
      messageHandler(engine, messageBus),
      tcpServer(),
      whitePlayer(nullptr),
      blackPlayer(nullptr),
      nextClientId(1),
      gameStarted(false),
      whiteReconnectToken(),
      blackReconnectToken(),
      expiredReconnectTokens(),
      gameWasStarted(false),
      reconnectionPending(false),
      closingExpiredGame(false),
      reservedColor(PieceColor::White),
      reconnectionDeadline(),
      lastReconnectSecondsSent(-1) {
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

    // Places the black back rank.
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

    // Places the black pawns.
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

    // Places the white pawns.
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

    // Places the white back rank.
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

// Creates an opaque token for one player slot.
std::string GameServer::createReconnectToken(
    int clientId
) const {
    std::random_device randomDevice;
    std::mt19937_64 generator(
        randomDevice()
    );
    std::uniform_int_distribution<
        unsigned long long
    > distribution;

    std::ostringstream token;
    token
        << std::hex
        << distribution(generator)
        << distribution(generator)
        << static_cast<unsigned long long>(
            clientId
        );

    return token.str();
}

// Creates a session and assigns an available or reserved color.
std::shared_ptr<ClientSession>
GameServer::createSession(
    TcpConnection connection,
    const std::string& requestedToken
) {
    std::lock_guard<std::mutex> lock(
        playersMutex
    );

    const int clientId =
        nextClientId.fetch_add(1);

    if (closingExpiredGame) {
        Message fullMessage;
        fullMessage.type = MessageType::GameFull;
        fullMessage.accepted = false;
        fullMessage.reason = "game_closing";

        connection.sendMessage(
            Protocol::serialize(
                fullMessage
            )
        );
        connection.close();
        return nullptr;
    }

    if (
        !requestedToken.empty() &&
        expiredReconnectTokens.find(
            requestedToken
        ) != expiredReconnectTokens.end()
    ) {
        Message closedMessage;
        closedMessage.type =
            MessageType::GameClosed;
        closedMessage.accepted = false;
        closedMessage.reason =
            "opponent_did_not_reconnect";

        connection.sendMessage(
            Protocol::serialize(
                closedMessage
            )
        );
        connection.close();
        return nullptr;
    }

    if (reconnectionPending) {
        const std::string& reservedToken =
            reservedColor == PieceColor::White
                ? whiteReconnectToken
                : blackReconnectToken;

        if (
            requestedToken == reservedToken &&
            !requestedToken.empty()
        ) {
            std::shared_ptr<ClientSession> session =
                std::make_shared<ClientSession>(
                    clientId,
                    reservedColor,
                    std::move(connection)
                );

            if (reservedColor == PieceColor::White) {
                whitePlayer = session;
            }
            else {
                blackPlayer = session;
            }

            reconnectionPending = false;
            lastReconnectSecondsSent = -1;

            std::cout
                << "Client "
                << clientId
                << " reclaimed "
                << (
                    reservedColor ==
                            PieceColor::White
                        ? "White"
                        : "Black"
                )
                << '\n';

            return session;
        }
    }

    if (
        !reconnectionPending &&
        (
            whitePlayer == nullptr ||
            !whitePlayer->isConnected()
        )
    ) {
        whiteReconnectToken =
            createReconnectToken(clientId);

        whitePlayer =
            std::make_shared<ClientSession>(
                clientId,
                PieceColor::White,
                std::move(connection)
            );

        std::cout
            << "Client "
            << clientId
            << " assigned to White\n";

        return whitePlayer;
    }

    if (
        !reconnectionPending &&
        (
            blackPlayer == nullptr ||
            !blackPlayer->isConnected()
        )
    ) {
        blackReconnectToken =
            createReconnectToken(clientId);

        blackPlayer =
            std::make_shared<ClientSession>(
                clientId,
                PieceColor::Black,
                std::move(connection)
            );

        std::cout
            << "Client "
            << clientId
            << " assigned to Black\n";

        return blackPlayer;
    }

    std::cout
        << "Connection rejected: game is full\n";

    Message gameFullMessage;
    gameFullMessage.type = MessageType::GameFull;
    gameFullMessage.accepted = false;
    gameFullMessage.reason = "game_full";

    connection.sendMessage(
        Protocol::serialize(gameFullMessage)
    );
    connection.close();

    return nullptr;
}

bool GameServer::hasTwoPlayers() {
    std::lock_guard<std::mutex> lock(
        playersMutex
    );

    return
        whitePlayer != nullptr &&
        whitePlayer->isConnected() &&
        blackPlayer != nullptr &&
        blackPlayer->isConnected();
}

// Sends a message to every connected player.
void GameServer::broadcast(
    const std::string& serializedMessage
) {
    std::vector<
        std::shared_ptr<ClientSession>
    > sessions;

    {
        std::lock_guard<std::mutex> lock(
            playersMutex
        );

        if (
            whitePlayer != nullptr &&
            whitePlayer->isConnected()
        ) {
            sessions.push_back(whitePlayer);
        }

        if (
            blackPlayer != nullptr &&
            blackPlayer->isConnected()
        ) {
            sessions.push_back(blackPlayer);
        }
    }

    for (const auto& session : sessions) {
        try {
            session->sendMessage(
                serializedMessage
            );
        }
        catch (const std::exception& exception) {
            std::cerr
                << "Broadcast failed for client "
                << session->getClientId()
                << ": "
                << exception.what()
                << '\n';

            session->disconnect();
            removeSession(session);
        }
    }
}

// Removes a disconnected session from the server.
void GameServer::removeSession(
    const std::shared_ptr<ClientSession>& session
) {
    std::shared_ptr<ClientSession>
        remainingPlayer;
    bool removed = false;
    bool startReconnection = false;

    {
        std::lock_guard<std::mutex> lock(
            playersMutex
        );

        if (whitePlayer == session) {
            whitePlayer.reset();
            removed = true;
        }

        if (blackPlayer == session) {
            blackPlayer.reset();
            removed = true;
        }

        if (removed) {
            gameStarted.store(false);

            remainingPlayer =
                whitePlayer != nullptr
                    ? whitePlayer
                    : blackPlayer;

            if (
                gameWasStarted &&
                !closingExpiredGame &&
                remainingPlayer != nullptr &&
                remainingPlayer->isConnected()
            ) {
                reservedColor =
                    session->getAssignedColor();
                reconnectionPending = true;
                reconnectionDeadline =
                    std::chrono::steady_clock::now() +
                    std::chrono::seconds(20);
                lastReconnectSecondsSent = -1;
                startReconnection = true;
            }
            else if (!gameWasStarted) {
                if (
                    session->getAssignedColor() ==
                    PieceColor::White
                ) {
                    whiteReconnectToken.clear();
                }
                else {
                    blackReconnectToken.clear();
                }
            }
        }
    }

    if (startReconnection) {
        sendReconnectingStatus(20);
        return;
    }

    if (
        remainingPlayer != nullptr &&
        remainingPlayer->isConnected()
    ) {
        Message waitingMessage;
        waitingMessage.type =
            MessageType::WaitingForOpponent;
        waitingMessage.accepted = true;
        waitingMessage.reason =
            "waiting_for_opponent";

        try {
            remainingPlayer->sendMessage(
                Protocol::serialize(
                    waitingMessage
                )
            );
        }
        catch (...) {
            remainingPlayer->disconnect();
        }
    }
}

// Sends the server-authoritative reconnection countdown.
void GameServer::sendReconnectingStatus(
    int secondsRemaining
) {
    Message reconnectingMessage;
    reconnectingMessage.type =
        MessageType::Reconnecting;
    reconnectingMessage.accepted = true;
    reconnectingMessage.reason =
        "opponent_disconnected";
    reconnectingMessage.secondsRemaining =
        secondsRemaining;

    broadcast(
        Protocol::serialize(
            reconnectingMessage
        )
    );
}

// Closes an expired game and prepares the server for new players.
void GameServer::closeExpiredGame() {
    std::vector<
        std::shared_ptr<ClientSession>
    > sessions;

    {
        std::lock_guard<std::mutex> lock(
            playersMutex
        );

        closingExpiredGame = true;
        reconnectionPending = false;
        gameStarted.store(false);

        if (!whiteReconnectToken.empty()) {
            expiredReconnectTokens.insert(
                whiteReconnectToken
            );
        }
        if (!blackReconnectToken.empty()) {
            expiredReconnectTokens.insert(
                blackReconnectToken
            );
        }

        if (whitePlayer != nullptr) {
            sessions.push_back(whitePlayer);
        }

        if (blackPlayer != nullptr) {
            sessions.push_back(blackPlayer);
        }
    }

    Message closedMessage;
    closedMessage.type = MessageType::GameClosed;
    closedMessage.accepted = false;
    closedMessage.reason =
        "opponent_did_not_reconnect";

    const std::string serializedMessage =
        Protocol::serialize(closedMessage);

    for (const auto& session : sessions) {
        try {
            session->sendMessage(
                serializedMessage
            );
        }
        catch (...) {
        }

        session->disconnect();
    }

    {
        std::lock_guard<std::mutex> engineLock(
            engineMutex
        );

        engine = GameEngine(
            createInitialBoard()
        );
        messageBus.clear();
    }

    {
        std::lock_guard<std::mutex> lock(
            playersMutex
        );

        whitePlayer.reset();
        blackPlayer.reset();
        whiteReconnectToken.clear();
        blackReconnectToken.clear();
        gameWasStarted = false;
        closingExpiredGame = false;
        lastReconnectSecondsSent = -1;
    }
}

// Updates or expires a pending reconnection period.
void GameServer::updateReconnectionState() {
    int secondsRemaining = -1;
    bool expired = false;

    {
        std::lock_guard<std::mutex> lock(
            playersMutex
        );

        if (!reconnectionPending) {
            return;
        }

        const auto now =
            std::chrono::steady_clock::now();

        if (now >= reconnectionDeadline) {
            expired = true;
        }
        else {
            const auto remaining =
                reconnectionDeadline - now;

            secondsRemaining =
                static_cast<int>(
                    std::chrono::duration_cast<
                        std::chrono::seconds
                    >(
                        remaining +
                        std::chrono::milliseconds(
                            999
                        )
                    ).count()
                );

            if (
                secondsRemaining ==
                lastReconnectSecondsSent
            ) {
                return;
            }

            lastReconnectSecondsSent =
                secondsRemaining;
        }
    }

    if (expired) {
        closeExpiredGame();
    }
    else {
        sendReconnectingStatus(
            secondsRemaining
        );
    }
}

// Handles requests from one connected client.
void GameServer::handleClient(
    const std::shared_ptr<ClientSession>& session
) {
    int processedRequests = 0;

    Message assignment;

    assignment.type =
        MessageType::PlayerAssigned;

    assignment.sequence = 0;
    assignment.accepted = true;
    assignment.reason = "player_assigned";

    assignment.playerColor =
        session->getAssignedColor() ==
                PieceColor::White
            ? "white"
            : "black";

    {
        std::lock_guard<std::mutex> lock(
            playersMutex
        );

        assignment.reconnectToken =
            session->getAssignedColor() ==
                    PieceColor::White
                ? whiteReconnectToken
                : blackReconnectToken;
    }

    assignment.createdAtMs = 0;
    assignment.hasSnapshot = false;

    session->sendMessage(
        Protocol::serialize(assignment)
    );

    Message initialUpdate;

    {
        std::lock_guard<std::mutex> lock(
            engineMutex
        );

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
    }

    session->sendMessage(
        Protocol::serialize(initialUpdate)
    );

    std::cout
        << "Initial game state sent to client "
        << session->getClientId()
        << '\n';

    if (hasTwoPlayers()) {
        const bool wasStarted =
            gameStarted.exchange(true);

        if (!wasStarted) {
            {
                std::lock_guard<std::mutex> lock(
                    playersMutex
                );
                gameWasStarted = true;
                reconnectionPending = false;
            }

            Message startedMessage;
            startedMessage.type =
                MessageType::GameStarted;
            startedMessage.accepted = true;
            startedMessage.reason =
                "game_started";

            broadcast(
                Protocol::serialize(
                    startedMessage
                )
            );
        }
    }
    else {
        Message waitingMessage;
        waitingMessage.type =
            MessageType::WaitingForOpponent;
        waitingMessage.accepted = true;
        waitingMessage.reason =
            "waiting_for_opponent";

        session->sendMessage(
            Protocol::serialize(
                waitingMessage
            )
        );
    }

    while (session->isConnected()) {
        const bool requestAvailable =
            session->getConnection()
                .waitForIncomingData(100);

        if (!requestAvailable) {
            continue;
        }

        const std::string requestJson =
            session->getConnection()
                .receiveMessage();

        if (requestJson.empty()) {
            std::cout
                << "Client "
                << session->getClientId()
                << " disconnected\n";

            break;
        }

        const Message request =
            Protocol::deserialize(
                requestJson
            );

        std::cout
            << "Request received from client "
            << session->getClientId()
            << ". Sequence: "
            << request.sequence
            << '\n';

        Message response;

        {
            std::lock_guard<std::mutex> lock(
                engineMutex
            );

            const std::shared_ptr<Piece> sourcePiece =
                engine.getBoard().getPieceAt(
                    request.source
                );

            if (
                !gameStarted.load()
            ) {
                response.type =
                    MessageType::MoveRejected;
                response.sequence =
                    request.sequence;
                response.source = request.source;
                response.destination =
                    request.destination;
                response.accepted = false;
                response.reason =
                    "waiting_for_opponent";
                response.createdAtMs =
                    engine.getCurrentTimeMs();
                response.hasSnapshot = true;
                response.snapshot =
                    GameStateSnapshotBuilder::build(
                        engine
                    );
            }
            else if (
                sourcePiece != nullptr &&
                sourcePiece->getColor() !=
                    session->getAssignedColor()
            ) {
                response.type =
                    MessageType::MoveRejected;

                response.sequence =
                    request.sequence;

                response.source =
                    request.source;

                response.destination =
                    request.destination;

                response.accepted = false;
                response.reason =
                    "not_your_piece";

                response.createdAtMs =
                    engine.getCurrentTimeMs();

                response.hasSnapshot = true;

                response.snapshot =
                    GameStateSnapshotBuilder::build(
                        engine
                    );
            }
            else {
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

                response =
                    messageBus.receiveForClient();
            }
        }

        session->sendMessage(
            Protocol::serialize(response)
        );

        ++processedRequests;

        std::cout
            << "Response sent to client "
            << session->getClientId()
            << ". Accepted: "
            << (
                response.accepted
                    ? "true"
                    : "false"
            )
            << ", reason: "
            << response.reason
            << '\n';
    }

    session->disconnect();
    removeSession(session);

    std::cout
        << "Processed requests from client "
        << session->getClientId()
        << ": "
        << processedRequests
        << '\n';
}

// Advances the game clock and broadcasts authoritative states.
void GameServer::runGameLoop() {
    auto previousTime =
        std::chrono::steady_clock::now();

    auto lastStateUpdate =
        previousTime;

    constexpr long long
        LOOP_INTERVAL_MS = 16;

    constexpr long long
        STATE_UPDATE_INTERVAL_MS = 50;

    while (true) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                LOOP_INTERVAL_MS
            )
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

        updateReconnectionState();

        if (!gameStarted.load()) {
            continue;
        }

        Message stateUpdate;
        bool shouldBroadcast = false;

        {
            std::lock_guard<std::mutex> lock(
                engineMutex
            );

            if (deltaMs > 0) {
                engine.wait(deltaMs);
            }

            const long long timeSinceUpdateMs =
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(
                    currentTime - lastStateUpdate
                ).count();

            if (
                timeSinceUpdateMs >=
                STATE_UPDATE_INTERVAL_MS
            ) {
                stateUpdate.type =
                    MessageType::GameStateUpdated;

                stateUpdate.sequence = 0;
                stateUpdate.accepted = true;
                stateUpdate.reason =
                    "state_update";

                stateUpdate.createdAtMs =
                    engine.getCurrentTimeMs();

                stateUpdate.hasSnapshot = true;

                stateUpdate.snapshot =
                    GameStateSnapshotBuilder::build(
                        engine
                    );

                lastStateUpdate = currentTime;
                shouldBroadcast = true;
            }
        }

        if (shouldBroadcast) {
            broadcast(
                Protocol::serialize(
                    stateUpdate
                )
            );
        }
    }
}

// Starts the game loop and accepts clients continuously.
void GameServer::run() {
    tcpServer.start(port);

    std::cout
        << "Game server listening on 0.0.0.0:"
        << port
        << '\n'
        << "Clients should connect using this "
        << "computer's IPv4 address.\n";

    std::thread gameLoopThread(
        &GameServer::runGameLoop,
        this
    );

    gameLoopThread.detach();

    while (true) {
        std::cout
            << "Waiting for client...\n";

        try {
            TcpConnection connection =
                tcpServer.acceptClient();

            const std::string handshakeJson =
                connection.receiveMessage();

            if (handshakeJson.empty()) {
                connection.close();
                continue;
            }

            const Message handshake =
                Protocol::deserialize(
                    handshakeJson
                );

            if (
                handshake.type !=
                MessageType::ReconnectRequest
            ) {
                throw std::runtime_error(
                    "Client did not send a reconnect handshake"
                );
            }

            std::shared_ptr<ClientSession> session =
                createSession(
                    std::move(connection),
                    handshake.reconnectToken
                );

            if (session == nullptr) {
                continue;
            }

            std::thread clientThread(
                [this, session]() {
                    try {
                        handleClient(session);
                    }
                    catch (
                        const std::exception& exception
                    ) {
                        std::cerr
                            << "Client "
                            << session->getClientId()
                            << " error: "
                            << exception.what()
                            << '\n';

                        session->disconnect();
                        removeSession(session);
                    }
                }
            );

            clientThread.detach();
        }
        catch (const std::exception& exception) {
            std::cerr
                << "Accept error: "
                << exception.what()
                << '\n';
        }
    }
}
