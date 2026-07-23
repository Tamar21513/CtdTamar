#include "../include/Core/Board.hpp"
#include "../include/Core/Piece.hpp"
#include "../include/Engine/GameEngine.hpp"
#include "../include/Messaging/Message.hpp"
#include "../include/Network/Protocol.hpp"

#include <cassert>
#include <iostream>
#include <memory>

using namespace std;

// Tests that the engine accepts a valid move received as a Message.
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

    Message request;

    request.type = MessageType::MoveRequest;
    request.sequence = 1;
    request.source = Position(6, 4);
    request.destination = Position(5, 4);
    request.accepted = false;
    request.reason = "";
    request.createdAtMs = 0;

    const string requestJson =
        Protocol::serialize(request);

    const Message restoredRequest =
        Protocol::deserialize(requestJson);

    const Message response =
        engine.handleMessage(restoredRequest);

    assert(response.type == MessageType::MoveAccepted);
    assert(response.accepted);
    assert(response.sequence == request.sequence);
    assert(response.source == request.source);
    assert(response.destination == request.destination);

    const string responseJson =
        Protocol::serialize(response);

    cout << "Engine message test passed\n";
    cout << "Request:  " << requestJson << '\n';
    cout << "Response: " << responseJson << '\n';

    return 0;
}
