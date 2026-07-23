#include "../include/Core/Board.hpp"
#include "../include/Core/Piece.hpp"
#include "../include/Engine/GameEngine.hpp"
#include "../include/Messaging/EngineMessageHandler.hpp"
#include "../include/Messaging/MessageBus.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

using namespace std;

// Tests the full request and response flow through MessageBus.
int main() {
    Board board(8, 8);

    board.placePiece(
        Position(6, 4),
        make_shared<Piece>(
            1,
            PieceColor::White,
            PieceKind::Pawn
        )
    );

    GameEngine engine(std::move(board));
    MessageBus messageBus;

    EngineMessageHandler handler(
        engine,
        messageBus
    );

    Message request;

    request.type = MessageType::MoveRequest;
    request.sequence = 25;
    request.source = Position(6, 4);
    request.destination = Position(5, 4);
    request.createdAtMs = 0;

    messageBus.sendToEngine(request);

    assert(messageBus.hasMessageForEngine());
    assert(handler.processNextMessage());
    assert(!messageBus.hasMessageForEngine());
    assert(messageBus.hasMessageForClient());

    const Message response =
        messageBus.receiveForClient();

    assert(response.sequence == 25);
    assert(response.type == MessageType::MoveAccepted);
    assert(response.accepted);
    assert(response.source == Position(6, 4));
    assert(response.destination == Position(5, 4));

    cout << "EngineMessageHandler test passed\n";

    return 0;
}