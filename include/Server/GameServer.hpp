#ifndef GAME_SERVER_HPP
#define GAME_SERVER_HPP

// Windows socket headers must be included first.
#include "../Network/TcpConnection.hpp"
#include "../Network/TcpServer.hpp"

#include "ClientSession.hpp"

#include "../Core/Board.hpp"
#include "../Engine/GameEngine.hpp"
#include "../Messaging/EngineMessageHandler.hpp"
#include "../Messaging/EventBus.hpp"
#include "../Messaging/GameEventSubscribers.hpp"
#include "../Messaging/MessageBus.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

class GameServer {
private:
    unsigned short port;

    EventBus eventBus;
    MoveHistorySubscriber moveHistorySubscriber;
    ScoreSubscriber scoreSubscriber;
    LifecycleSubscriber lifecycleSubscriber;
    GameLifecyclePublisher lifecyclePublisher;
    GameEngine engine;
    MessageBus messageBus;
    EngineMessageHandler messageHandler;
    TcpServer tcpServer;

    std::shared_ptr<ClientSession> whitePlayer;
    std::shared_ptr<ClientSession> blackPlayer;

    std::mutex playersMutex;
    std::mutex engineMutex;

    std::atomic<int> nextClientId;
    std::atomic<bool> gameStarted;

    std::string whiteReconnectToken;
    std::string blackReconnectToken;
    std::string whiteUsername;
    std::string blackUsername;
    std::unordered_set<std::string>
        expiredReconnectTokens;
    bool gameWasStarted;
    bool reconnectionPending;
    bool closingExpiredGame;
    PieceColor reservedColor;
    std::chrono::steady_clock::time_point
        reconnectionDeadline;
    int lastReconnectSecondsSent;
    std::atomic<long long>
        gameplayStartsAtEpochMs;

    // Creates the standard initial chess board.
    static Board createInitialBoard();

    // Creates a session and assigns an available color.
    std::shared_ptr<ClientSession> createSession(
        TcpConnection connection,
        const std::string& requestedToken,
        const std::string& requestedUsername
    );

    GameStateSnapshot buildSnapshot();
    bool isGameplayEnabled() const;

    // Handles requests from one connected client.
    void handleClient(
        const std::shared_ptr<ClientSession>& session
    );

    // Advances the game clock and broadcasts game states.
    void runGameLoop();

    // Sends a serialized message to all connected players.
    void broadcast(
        const std::string& serializedMessage
    );

    // Removes a disconnected client from its player slot.
    void removeSession(
        const std::shared_ptr<ClientSession>& session
    );

    // Returns whether both player slots are connected.
    bool hasTwoPlayers();

    // Creates an opaque token for one player slot.
    std::string createReconnectToken(
        int clientId
    ) const;

    // Sends the current reconnection countdown.
    void sendReconnectingStatus(int secondsRemaining);

    // Updates or expires a pending reconnection period.
    void updateReconnectionState();

    // Closes an expired game and prepares for new players.
    void closeExpiredGame();

public:
    // Creates a game server on the requested port.
    explicit GameServer(unsigned short port);

    // Starts the permanent server loop.
    void run();
};

#endif
