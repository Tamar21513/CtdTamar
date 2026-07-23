#ifndef GAME_SERVER_HPP
#define GAME_SERVER_HPP

// Windows socket headers must be included first.
#include "../Network/TcpConnection.hpp"
#include "../Network/TcpServer.hpp"

#include "../Core/Board.hpp"
#include "../Engine/GameEngine.hpp"
#include "../Messaging/EngineMessageHandler.hpp"
#include "../Messaging/MessageBus.hpp"

class GameServer {
private:
    unsigned short port;

    GameEngine engine;
    MessageBus messageBus;
    EngineMessageHandler messageHandler;
    TcpServer tcpServer;

    // Creates the standard initial chess board.
    static Board createInitialBoard();

    // Handles all requests from one connected client.
    void handleClient(TcpConnection& client);

public:
    explicit GameServer(unsigned short port);

    // Starts the permanent server loop.
    void run();
};

#endif