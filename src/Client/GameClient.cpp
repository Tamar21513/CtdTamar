#include "../../include/Client/GameClient.hpp"

#include "../../include/Network/Protocol.hpp"

#include <chrono>
#include <stdexcept>

GameClient::GameClient()
    : connection(),
      connected(false),
      nextSequence(1),
      receiverThread(),
      incomingMessages(),
      connectionError() {
}

GameClient::~GameClient() {
    disconnect();
}

// Connects once and starts a permanent receiving thread.
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
        std::lock_guard<std::mutex> lock(incomingMutex);

        incomingMessages.clear();
        connectionError.clear();
    }

    connection.connectTo(ip, port);
    connected.store(true);

    receiverThread =
        std::thread(&GameClient::receiveLoop, this);
}

// Continuously receives both responses and server-initiated updates.
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

            {
                std::lock_guard<std::mutex> lock(
                    incomingMutex
                );

                incomingMessages.push_back(message);
            }

            incomingCondition.notify_all();
        }
    }
    catch (const std::exception& exception) {
        {
            std::lock_guard<std::mutex> lock(
                incomingMutex
            );

            // Do not replace a deliberate disconnect
            // with a receive error.
            if (connected.load()) {
                connectionError = exception.what();
            }
        }

        connected.store(false);
        incomingCondition.notify_all();
    }
}

// Stops reception and closes the socket.
void GameClient::disconnect() {
    connected.store(false);

    // Closing the socket releases receiveMessage()
    // if the receiver thread is waiting inside recv().
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

bool GameClient::isConnected() const {
    return connected.load();
}

// Returns a unique sequence number.
unsigned long long GameClient::createSequence() {
    return nextSequence.fetch_add(1);
}

// Sends a request while reception continues in the background.
void GameClient::send(const Message& message) {
    if (!connected.load()) {
        throw std::runtime_error(
            "GameClient is not connected"
        );
    }

    const std::string json =
        Protocol::serialize(message);

    std::lock_guard<std::mutex> lock(sendingMutex);

    try {
        connection.sendMessage(json);
    }
    catch (...) {
        connected.store(false);
        incomingCondition.notify_all();
        throw;
    }
}

// A response must have the same sequence
// and be either accepted or rejected.
bool GameClient::isMatchingResponse(
    const Message& message,
    unsigned long long sequence
) {
    if (message.sequence != sequence) {
        return false;
    }

    return
        message.type == MessageType::MoveAccepted ||
        message.type == MessageType::MoveRejected;
}

// Waits for the response belonging to one request.
// Other updates remain in the queue.
Message GameClient::waitForResponse(
    unsigned long long sequence,
    long long timeoutMs
) {
    if (timeoutMs <= 0) {
        throw std::invalid_argument(
            "Timeout must be positive"
        );
    }

    std::unique_lock<std::mutex> lock(incomingMutex);

    const auto hasResponseOrDisconnected =
        [this, sequence]() {
            for (const Message& message :
                 incomingMessages) {
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
            std::chrono::milliseconds(timeoutMs),
            hasResponseOrDisconnected
        );

    if (!awakened) {
        throw std::runtime_error(
            "Timed out while waiting for server response"
        );
    }

    for (
        auto iterator = incomingMessages.begin();
        iterator != incomingMessages.end();
        ++iterator
    ) {
        if (
            isMatchingResponse(
                *iterator,
                sequence
            )
        ) {
            const Message response = *iterator;
            incomingMessages.erase(iterator);
            return response;
        }
    }

    if (!connectionError.empty()) {
        throw std::runtime_error(connectionError);
    }

    throw std::runtime_error(
        "Connection closed before the response arrived"
    );
}

// Returns the next server-initiated update.
// Move responses are left for waitForResponse().
bool GameClient::tryReceiveUpdate(Message& message) {
    std::lock_guard<std::mutex> lock(incomingMutex);

    for (
        auto iterator = incomingMessages.begin();
        iterator != incomingMessages.end();
        ++iterator
    ) {
        if (
            iterator->type != MessageType::MoveAccepted &&
            iterator->type != MessageType::MoveRejected
        ) {
            message = *iterator;
            incomingMessages.erase(iterator);
            return true;
        }
    }

    return false;
}

std::string GameClient::getConnectionError() const {
    std::lock_guard<std::mutex> lock(incomingMutex);
    return connectionError;
}