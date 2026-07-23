#include "../../include/Messaging/EngineMessageHandler.hpp"

#include "../../include/Messaging/GameStateSnapshotBuilder.hpp"

// Creates a handler connected to the engine and message bus.
EngineMessageHandler::EngineMessageHandler(
    GameEngine& engine,
    MessageBus& messageBus
) : engine(engine),
    messageBus(messageBus) {
}

// Processes one request and adds
// the authoritative game snapshot to its response.
bool EngineMessageHandler::processNextMessage() {
    if (!messageBus.hasMessageForEngine()) {
        return false;
    }

    const Message request =
        messageBus.receiveForEngine();

    Message response =
        engine.handleMessage(request);

    response.hasSnapshot = true;

    response.snapshot =
        GameStateSnapshotBuilder::build(
            engine
        );

    messageBus.sendToClient(response);

    return true;
}

// Processes every request currently waiting for the engine.
void EngineMessageHandler::processAllMessages() {
    while (processNextMessage()) {
        // Continue until the engine queue is empty.
    }
}