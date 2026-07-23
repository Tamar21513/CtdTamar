#ifndef MESSAGE_BUS_HPP
#define MESSAGE_BUS_HPP

#include <cstddef>
#include <deque>

#include "Message.hpp"

class MessageBus {
private:
    std::deque<Message> clientToEngine;
    std::deque<Message> engineToClient;

public:
    void sendToEngine(const Message& message);
    bool hasMessageForEngine() const;
    Message receiveForEngine();

    void sendToClient(const Message& message);
    bool hasMessageForClient() const;
    Message receiveForClient();

    std::size_t pendingEngineMessages() const;
    std::size_t pendingClientMessages() const;

    void clear();
};

#endif