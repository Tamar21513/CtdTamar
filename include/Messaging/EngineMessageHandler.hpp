#ifndef ENGINE_MESSAGE_HANDLER_HPP
#define ENGINE_MESSAGE_HANDLER_HPP

#include "../Engine/GameEngine.hpp"
#include "MessageBus.hpp"

class EngineMessageHandler {
private:
    GameEngine& engine;
    MessageBus& messageBus;

public:
    // Creates a handler connected to the engine and message bus.
    EngineMessageHandler(
        GameEngine& engine,
        MessageBus& messageBus
    );

    // Processes the oldest request waiting for the engine.
    bool processNextMessage();

    // Processes all requests currently waiting for the engine.
    void processAllMessages();
};

#endif