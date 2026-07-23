#ifndef GAME_CLIENT_HPP
#define GAME_CLIENT_HPP

#include "../Network/TcpConnection.hpp"
#include "../Messaging/Message.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class GameClient {
private:
    TcpConnection connection;

    std::atomic<bool> connected;
    std::atomic<unsigned long long> nextSequence;

    std::thread receiverThread;

    mutable std::mutex incomingMutex;
    std::mutex sendingMutex;
    std::condition_variable incomingCondition;

    std::deque<Message> incomingMessages;
    std::string connectionError;

    // Continuously receives messages from the server.
    void receiveLoop();

    // Checks whether a message is the response for a request.
    static bool isMatchingResponse(
        const Message& message,
        unsigned long long sequence
    );

public:
    GameClient();

    GameClient(const GameClient&) = delete;
    GameClient& operator=(const GameClient&) = delete;

    ~GameClient();

    // Connects to the server and starts continuous reception.
    void connectTo(
        const std::string& ip,
        unsigned short port
    );

    // Disconnects and stops the receiver thread.
    void disconnect();

    bool isConnected() const;

    // Generates a sequence number for a new request.
    unsigned long long createSequence();

    // Sends a message without blocking the receiver.
    void send(const Message& message);

    // Waits for the response matching a specific request.
    Message waitForResponse(
        unsigned long long sequence,
        long long timeoutMs
    );

    // Retrieves an unsolicited server update, if available.
    bool tryReceiveUpdate(Message& message);

    // Returns the latest connection error.
    std::string getConnectionError() const;
};

#endif