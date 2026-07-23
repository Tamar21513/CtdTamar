#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <optional>

#include "../Core/Position.hpp"
#include "../Core/Results.hpp"
#include "../Engine/GameEngine.hpp"
#include "../IO/BoardMapper.hpp"
#include "../Messaging/MessageBus.hpp"
#include "../Messaging/EngineMessageHandler.hpp"

using namespace std;

class Controller {
private:
    GameEngine& engine;
    MessageBus messageBus;
    EngineMessageHandler messageHandler;

    unsigned long long nextSequence;

    optional<Position> selectedCell;

    int boardStartX;
    int boardStartY;

    int cellSizeX;
    int cellSizeY;

    // Converts a window pixel coordinate into a board position.
    optional<Position> mapClickToCell(
        int x,
        int y
    ) const;

    // Checks whether a piece is available for selection.
    bool isPieceAvailable(
        const shared_ptr<Piece>& piece
    ) const;

    // Handles the first piece selection.
    ControllerResult selectFirstPiece(
        const Position& clickedPosition
    );

    // Handles a click while another cell is selected.
    ControllerResult handleSelectedCell(
        const Position& clickedPosition
    );

    // Replaces the current selection with a friendly piece.
    ControllerResult selectFriendlyPiece(
        const Position& clickedPosition
    );

    // Sends a move request for the currently selected piece.
    ControllerResult requestSelectedMove(
        const Position& source,
        const Position& destination
    );

    // Sends a request through MessageBus and returns the engine response.
    ControllerResult sendRequest(
        MessageType type,
        const Position& source,
        const Position& destination
    );

public:
    // Creates a controller with the default board geometry.
    Controller(GameEngine& engine);

    // Creates a controller with the supplied visual board geometry.
    Controller(
        GameEngine& engine,
        int boardStartX,
        int boardStartY,
        int cellSizeX,
        int cellSizeY
    );

    // Converts one click into a selection, move, jump, or cancellation.
    ControllerResult click(
        int x,
        int y
    );

    // Handles an explicit jump command.
    ControllerResult jump(
        int x,
        int y
    );

    // Reports whether a source cell is currently selected.
    bool hasSelection() const;

    // Returns the currently selected source cell.
    optional<Position> getSelectedCell() const;
};

#endif