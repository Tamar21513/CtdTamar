#include "../../include/Client/GameClient.hpp"

#include "../../include/Network/Protocol.hpp"
#include "../../include/Core/Piece.hpp"

#include <chrono>
#include <cstdlib>
#include <stdexcept>

// Creates a disconnected game client.
GameClient::GameClient()
    : connection(),
      connected(false),
      playerAssigned(false),
      gameStarted(false),
      gameFull(false),
      reconnecting(false),
      gameClosed(false),
      reconnectSecondsRemaining(0),
      assignedColor(PieceColor::White),
      nextSequence(1),
      receiverThread(),
      incomingMessages(),
      connectionError(),
      reconnectToken() {
    const char* token =
        std::getenv("CTD_RECONNECT_TOKEN");

    if (token != nullptr) {
        reconnectToken = token;
    }
}

// Disconnects and destroys the game client.
GameClient::~GameClient() {
    disconnect();
}

// Connects to the server and starts the receiving thread.
void GameClient::connectTo(
    const std::string& ip,
    unsigned short port
) {
    if (connected.load()) {
        throw std::runtime_error(
            "GameClient is already connected"
        );
    }

    if (receiverThread.joinable()) {
        receiverThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(
            incomingMutex
        );

        incomingMessages.clear();
        connectionError.clear();
    }

    playerAssigned.store(false);
    gameStarted.store(false);
    gameFull.store(false);
    reconnecting.store(false);
    gameClosed.store(false);
    reconnectSecondsRemaining.store(0);
    assignedColor.store(PieceColor::White);
    nextSequence.store(1);

    connection.connectTo(ip, port);
    connected.store(true);

    Message reconnectRequest;
    reconnectRequest.type =
        MessageType::ReconnectRequest;

    {
        std::lock_guard<std::mutex> lock(
            incomingMutex
        );
        reconnectRequest.reconnectToken =
            reconnectToken;
    }

    connection.sendMessage(
        Protocol::serialize(
            reconnectRequest
        )
    );

    receiverThread =
        std::thread(
            &GameClient::receiveLoop,
            this
        );
}

// Continuously receives responses and updates from the server.
void GameClient::receiveLoop() {
    try {
        while (connected.load()) {
            const std::string json =
                connection.receiveMessage();

            if (json.empty()) {
                {
                    std::lock_guard<std::mutex> lock(
                        incomingMutex
                    );

                    connectionError =
                        "Server disconnected";
                }

                connected.store(false);
                incomingCondition.notify_all();
                return;
            }

            const Message message =
                Protocol::deserialize(json);

            // Player assignment is handled separately
            // and is not inserted into the message queue.
            if (
                message.type ==
                MessageType::PlayerAssigned
            ) {
                if (
                    message.playerColor ==
                    "white"
                ) {
                    assignedColor.store(
                        PieceColor::White
                    );
                }
                else if (
                    message.playerColor ==
                    "black"
                ) {
                    assignedColor.store(
                        PieceColor::Black
                    );
                }
                else {
                    throw std::runtime_error(
                        "Server assigned an invalid player color"
                    );
                }

                playerAssigned.store(true);

                {
                    std::lock_guard<std::mutex> lock(
                        incomingMutex
                    );
                    reconnectToken =
                        message.reconnectToken;
                }

                incomingCondition.notify_all();
                continue;
            }

            if (
                message.type ==
                MessageType::WaitingForOpponent
            ) {
                gameStarted.store(false);
                reconnecting.store(false);
                reconnectSecondsRemaining.store(0);
            }
            else if (
                message.type ==
                MessageType::GameStarted
            ) {
                gameStarted.store(true);
                reconnecting.store(false);
                reconnectSecondsRemaining.store(0);
            }
            else if (
                message.type ==
                MessageType::GameFull
            ) {
                gameStarted.store(false);
                gameFull.store(true);
            }
            else if (
                message.type ==
                MessageType::Reconnecting
            ) {
                gameStarted.store(false);
                reconnecting.store(true);
                reconnectSecondsRemaining.store(
                    message.secondsRemaining
                );
            }
            else if (
                message.type ==
                MessageType::GameClosed
            ) {
                gameStarted.store(false);
                reconnecting.store(false);
                gameClosed.store(true);
                reconnectSecondsRemaining.store(0);
            }

            {
                std::lock_guard<std::mutex> lock(
                    incomingMutex
                );

                incomingMessages.push_back(
                    message
                );
            }

            incomingCondition.notify_all();
        }
    }
    catch (const std::exception& exception) {
        {
            std::lock_guard<std::mutex> lock(
                incomingMutex
            );

            // Preserve an intentional disconnect.
            if (connected.load()) {
                connectionError =
                    exception.what();
            }
        }

        connected.store(false);
        incomingCondition.notify_all();
    }
}

// Disconnects from the server and stops the receiving thread.
void GameClient::disconnect() {
    connected.store(false);

    // Closing the socket releases receiveMessage()
    // if it is currently waiting for data.
    connection.close();

    incomingCondition.notify_all();

    if (
        receiverThread.joinable() &&
        receiverThread.get_id() !=
            std::this_thread::get_id()
    ) {
        receiverThread.join();
    }
}

// Returns whether the client is connected.
bool GameClient::isConnected() const {
    return connected.load();
}

// Returns whether the server assigned a player color.
bool GameClient::hasPlayerAssignment() const {
    return playerAssigned.load();
}

// Returns the player color assigned by the server.
PieceColor GameClient::getAssignedColor() const {
    if (!playerAssigned.load()) {
        throw std::runtime_error(
            "Player color has not been assigned"
        );
    }

    return assignedColor.load();
}

bool GameClient::hasGameStarted() const {
    return gameStarted.load();
}

bool GameClient::isGameFull() const {
    return gameFull.load();
}

// Returns whether the opponent may reconnect.
bool GameClient::isReconnecting() const {
    return reconnecting.load();
}

// Returns whether the reconnection window expired.
bool GameClient::isGameClosed() const {
    return gameClosed.load();
}

// Returns the countdown supplied by the server.
int GameClient::getReconnectSecondsRemaining()
    const {
    return reconnectSecondsRemaining.load();
}

// Returns the token used to reclaim the assigned color.
std::string GameClient::getReconnectToken() const {
    std::lock_guard<std::mutex> lock(
        incomingMutex
    );

    return reconnectToken;
}

// Creates a unique sequence number for a request.
unsigned long long GameClient::createSequence() {
    return nextSequence.fetch_add(1);
}

// Sends a message to the server.
void GameClient::send(
    const Message& message
) {
    if (!connected.load()) {
        throw std::runtime_error(
            "GameClient is not connected"
        );
    }

    const std::string json =
        Protocol::serialize(message);

    std::lock_guard<std::mutex> lock(
        sendingMutex
    );

    try {
        connection.sendMessage(json);
    }
    catch (...) {
        connected.store(false);
        incomingCondition.notify_all();
        throw;
    }
}

// Checks whether a message is the response for a request.
bool GameClient::isMatchingResponse(
    const Message& message,
    unsigned long long sequence
) {
    if (message.sequence != sequence) {
        return false;
    }

    return
        message.type ==
            MessageType::MoveAccepted ||
        message.type ==
            MessageType::MoveRejected;
}

// Waits for the response matching a specific request.
Message GameClient::waitForResponse(
    unsigned long long sequence,
    long long timeoutMs
) {
    if (timeoutMs <= 0) {
        throw std::invalid_argument(
            "Timeout must be positive"
        );
    }

    std::unique_lock<std::mutex> lock(
        incomingMutex
    );

    const auto hasResponseOrDisconnected =
        [this, sequence]() {
            for (
                const Message& message :
                incomingMessages
            ) {
                if (
                    isMatchingResponse(
                        message,
                        sequence
                    )
                ) {
                    return true;
                }
            }

            return !connected.load();
        };

    const bool awakened =
        incomingCondition.wait_for(
            lock,
            std::chrono::milliseconds(
                timeoutMs
            ),
            hasResponseOrDisconnected
        );

    if (!awakened) {
        throw std::runtime_error(
            "Timed out while waiting for server response"
        );
    }

    for (
        auto iterator =
            incomingMessages.begin();
        iterator != incomingMessages.end();
        ++iterator
    ) {
        if (
            isMatchingResponse(
                *iterator,
                sequence
            )
        ) {
            const Message response =
                *iterator;

            incomingMessages.erase(
                iterator
            );

            return response;
        }
    }

    if (!connectionError.empty()) {
        throw std::runtime_error(
            connectionError
        );
    }

    throw std::runtime_error(
        "Connection closed before the response arrived"
    );
}

// Retrieves the next server-initiated update.
bool GameClient::tryReceiveUpdate(
    Message& message
) {
    std::lock_guard<std::mutex> lock(
        incomingMutex
    );

    for (
        auto iterator =
            incomingMessages.begin();
        iterator != incomingMessages.end();
        ++iterator
    ) {
        if (
            iterator->type !=
                MessageType::MoveAccepted &&
            iterator->type !=
                MessageType::MoveRejected
        ) {
            message = *iterator;

            incomingMessages.erase(
                iterator
            );

            return true;
        }
    }

    return false;
}

// Returns the latest connection error.
std::string GameClient::getConnectionError()
    const {
    std::lock_guard<std::mutex> lock(
        incomingMutex
    );

    return connectionError;
}
