#ifndef NETWORK_CONTROLLER_HPP
#define NETWORK_CONTROLLER_HPP

#include "GameClient.hpp"
#include <optional>

#include "../Core/Position.hpp"
#include "../Core/Results.hpp"
#include "ClientGameState.hpp"

class NetworkController {
private:
    GameClient& client;
    ClientGameState& gameState;

    std::optional<Position> selectedCell;

    int boardStartX;
    int boardStartY;

    int cellSizeX;
    int cellSizeY;

    int boardRows;
    int boardCols;

    long long responseTimeoutMs;

    std::optional<Position> mapClickToCell(
        int x,
        int y
    ) const;

    bool isPieceAvailable(
        const PieceSnapshot* piece
    ) const;

    ControllerResult selectFirstPiece(
        const Position& clickedPosition
    );

    ControllerResult selectFriendlyPiece(
        const Position& clickedPosition
    );

    ControllerResult handleSelectedCell(
        const Position& clickedPosition
    );

    ControllerResult sendRequest(
        MessageType type,
        const Position& source,
        const Position& destination
    );

public:
    NetworkController(
        GameClient& client,
        ClientGameState& gameState,
        int boardStartX,
        int boardStartY,
        int cellSizeX,
        int cellSizeY,
        int boardRows = 8,
        int boardCols = 8,
        long long responseTimeoutMs = 5000
    );

    // Converts one mouse click into a selection,
    // move request, jump request, or cancellation.
    ControllerResult click(
        int x,
        int y
    );

    // Sends an explicit jump request.
    ControllerResult jump(
        int x,
        int y
    );

    // Applies updates sent by the server
    // without a matching client request.
    bool applyPendingUpdates();

    bool hasSelection() const;

    std::optional<Position>
    getSelectedCell() const;

    void clearSelection();
};

#endif