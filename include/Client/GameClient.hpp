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

enum class PieceColor;

class GameClient {
private:
    TcpConnection connection;

    std::atomic<bool> connected;
    std::atomic<bool> playerAssigned;
    std::atomic<bool> gameStarted;
    std::atomic<bool> gameFull;
    std::atomic<bool> reconnecting;
    std::atomic<bool> tryingToReconnect;
    std::atomic<bool> gameClosed;
    std::atomic<bool> stopRequested;
    std::atomic<int> reconnectSecondsRemaining;
    std::atomic<PieceColor> assignedColor;
    std::atomic<unsigned long long> nextSequence;

    std::thread receiverThread;

    mutable std::mutex incomingMutex;
    std::mutex sendingMutex;
    std::condition_variable incomingCondition;

    std::deque<Message> incomingMessages;
    std::string connectionError;
    std::string reconnectToken;
    std::string serverIp;
    unsigned short serverPort;

    // Continuously receives messages from the server.
    void receiveLoop();

    // Opens one connection and sends the saved token.
    bool connectWithSavedToken();

    // Marks a recoverable connection loss.
    void markConnectionLost(
        const std::string& error
    );

    // Waits before the next serial reconnect attempt.
    bool waitForReconnectAttempt();

    // Checks whether a message is the response for a request.
    static bool isMatchingResponse(
        const Message& message,
        unsigned long long sequence
    );

public:
    // Creates a disconnected game client.
    GameClient();

    GameClient(const GameClient&) = delete;
    GameClient& operator=(const GameClient&) = delete;

    // Disconnects and destroys the game client.
    ~GameClient();

    // Connects to the server and starts continuous reception.
    void connectTo(
        const std::string& ip,
        unsigned short port
    );

    // Disconnects and stops the receiver thread.
    void disconnect();

    // Returns whether the client is connected.
    bool isConnected() const;

    // Returns whether the server assigned a player color.
    bool hasPlayerAssignment() const;

    // Returns the color assigned by the server.
    PieceColor getAssignedColor() const;

    // Returns whether two players are currently connected.
    bool hasGameStarted() const;

    // Returns whether this connection was rejected as the third client.
    bool isGameFull() const;

    // Returns whether the server is waiting for a disconnected opponent.
    bool isReconnecting() const;

    // Returns whether this client is restoring its own connection.
    bool isTryingToReconnect() const;

    // Returns whether the reconnection period expired.
    bool isGameClosed() const;

    // Returns the server-authoritative reconnection countdown.
    int getReconnectSecondsRemaining() const;

    // Returns the token needed to reclaim this player's color.
    std::string getReconnectToken() const;

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
