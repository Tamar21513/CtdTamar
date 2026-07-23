#include "../../include/Messaging/MessageBus.hpp"

#include <stdexcept>

// Adds a client request to the engine queue.
void MessageBus::sendToEngine(const Message& message) {
    clientToEngine.push_back(message);
}

// Reports whether the engine has a message waiting.
bool MessageBus::hasMessageForEngine() const {
    return !clientToEngine.empty();
}

// Removes and returns the oldest engine message.
Message MessageBus::receiveForEngine() {
    if (clientToEngine.empty()) {
        throw std::runtime_error("No message available for engine");
    }

    Message message = clientToEngine.front();
    clientToEngine.pop_front();
    return message;
}

// Adds an engine response to the client queue.
void MessageBus::sendToClient(const Message& message) {
    engineToClient.push_back(message);
}

// Reports whether the client has a message waiting.
bool MessageBus::hasMessageForClient() const {
    return !engineToClient.empty();
}

// Removes and returns the oldest client message.
Message MessageBus::receiveForClient() {
    if (engineToClient.empty()) {
        throw std::runtime_error("No message available for client");
    }

    Message message = engineToClient.front();
    engineToClient.pop_front();
    return message;
}

// Returns the number of requests waiting for the engine.
std::size_t MessageBus::pendingEngineMessages() const {
    return clientToEngine.size();
}

// Returns the number of responses waiting for the client.
std::size_t MessageBus::pendingClientMessages() const {
    return engineToClient.size();
}

// Removes all queued messages.
void MessageBus::clear() {
    clientToEngine.clear();
    engineToClient.clear();
}