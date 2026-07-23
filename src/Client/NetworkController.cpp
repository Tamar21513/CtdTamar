#include "../../include/Client/NetworkController.hpp"

#include <stdexcept>
#include <string>

namespace {

bool haveSameColor(
    const PieceSnapshot& first,
    const PieceSnapshot& second
) {
    return
        !first.token.empty() &&
        !second.token.empty() &&
        first.token[0] == second.token[0];
}

} // namespace

NetworkController::NetworkController(
    GameClient& client,
    ClientGameState& gameState,
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY,
    int boardRows,
    int boardCols,
    long long responseTimeoutMs
) : client(client),
    gameState(gameState),
    selectedCell(std::nullopt),
    boardStartX(boardStartX),
    boardStartY(boardStartY),
    cellSizeX(cellSizeX),
    cellSizeY(cellSizeY),
    boardRows(boardRows),
    boardCols(boardCols),
    responseTimeoutMs(responseTimeoutMs) {

    if (
        cellSizeX <= 0 ||
        cellSizeY <= 0 ||
        boardRows <= 0 ||
        boardCols <= 0 ||
        responseTimeoutMs <= 0
    ) {
        throw std::invalid_argument(
            "Invalid NetworkController configuration"
        );
    }
}

// Converts window coordinates into a board position.
std::optional<Position>
NetworkController::mapClickToCell(
    int x,
    int y
) const {
    const int relativeX =
        x - boardStartX;

    const int relativeY =
        y - boardStartY;

    if (
        relativeX < 0 ||
        relativeY < 0
    ) {
        return std::nullopt;
    }

    const int col =
        relativeX / cellSizeX;

    const int row =
        relativeY / cellSizeY;

    if (
        row < 0 ||
        row >= boardRows ||
        col < 0 ||
        col >= boardCols
    ) {
        return std::nullopt;
    }

    return Position(row, col);
}

// Local availability is used only for selection.
// The server still performs the authoritative validation.
bool NetworkController::isPieceAvailable(
    const PieceSnapshot* piece
) const {
    return
        piece != nullptr &&
        piece->state == "idle" &&
        piece->remainingCooldownMs <= 0;
}

// Handles the first click.
ControllerResult
NetworkController::selectFirstPiece(
    const Position& clickedPosition
) {
    const PieceSnapshot* piece =
        gameState.findPieceAt(
            clickedPosition
        );

    if (piece == nullptr) {
        return {
            false,
            Reasons::EMPTY_CLICK
        };
    }

    if (!isPieceAvailable(piece)) {
        return {
            false,
            Reasons::MOTION_IN_PROGRESS
        };
    }

    selectedCell = clickedPosition;

    return {
        true,
        Reasons::SELECTED
    };
}

// Replaces the selection when another friendly
// available piece is clicked.
ControllerResult
NetworkController::selectFriendlyPiece(
    const Position& clickedPosition
) {
    const PieceSnapshot* piece =
        gameState.findPieceAt(
            clickedPosition
        );

    if (!isPieceAvailable(piece)) {
        selectedCell.reset();

        return {
            false,
            Reasons::MOTION_IN_PROGRESS
        };
    }

    selectedCell = clickedPosition;

    return {
        true,
        Reasons::SELECTED
    };
}

// Creates one network request and waits for
// the matching server response.
ControllerResult NetworkController::sendRequest(
    MessageType type,
    const Position& source,
    const Position& destination
) {
    if (!client.isConnected()) {
        return {
            false,
            "network_error: client_not_connected"
        };
    }

    Message request;

    request.type = type;

    request.sequence =
        client.createSequence();

    request.source = source;
    request.destination = destination;

    request.createdAtMs =
        gameState.getServerTimeMs();

    try {
        client.send(request);

        const Message response =
            client.waitForResponse(
                request.sequence,
                responseTimeoutMs
            );

        // A server response was received,
        // so this click operation has finished.
        selectedCell.reset();

        if (!response.hasSnapshot) {
            return {
                false,
                "missing_server_snapshot"
            };
        }

        gameState.applyMessage(response);

        return {
            response.accepted,
            response.reason
        };
    }
    catch (const std::exception& exception) {
        // Keep the existing ClientGameState unchanged.
        // The selection also remains, allowing another attempt.
        return {
            false,
            std::string("network_error: ") +
                exception.what()
        };
    }
}

// Handles the second click.
ControllerResult
NetworkController::handleSelectedCell(
    const Position& clickedPosition
) {
    const Position source =
        selectedCell.value();

    // Clicking the selected cell again requests a jump.
    if (clickedPosition == source) {
        return sendRequest(
            MessageType::JumpRequest,
            source,
            source
        );
    }

    const PieceSnapshot* selectedPiece =
        gameState.findPieceAt(source);

    if (selectedPiece == nullptr) {
        selectedCell.reset();

        return {
            false,
            Reasons::EMPTY_SOURCE
        };
    }

    if (!isPieceAvailable(selectedPiece)) {
        selectedCell.reset();

        return {
            false,
            Reasons::MOTION_IN_PROGRESS
        };
    }

    const PieceSnapshot* clickedPiece =
        gameState.findPieceAt(
            clickedPosition
        );

    if (
        clickedPiece != nullptr &&
        haveSameColor(
            *selectedPiece,
            *clickedPiece
        )
    ) {
        return selectFriendlyPiece(
            clickedPosition
        );
    }

    return sendRequest(
        MessageType::MoveRequest,
        source,
        clickedPosition
    );
}

ControllerResult NetworkController::click(
    int x,
    int y
) {
    // Apply server updates before interpreting the click.
    applyPendingUpdates();

    if (!gameState.hasSnapshot()) {
        return {
            false,
            "client_state_not_initialized"
        };
    }

    if (gameState.isGameOver()) {
        selectedCell.reset();

        return {
            false,
            Reasons::GAME_OVER
        };
    }

    const std::optional<Position> clickedCell =
        mapClickToCell(x, y);

    if (!clickedCell.has_value()) {
        const bool selectionWasCleared =
            selectedCell.has_value();

        selectedCell.reset();

        return {
            selectionWasCleared,
            Reasons::CLICK_OUTSIDE
        };
    }

    if (!selectedCell.has_value()) {
        return selectFirstPiece(
            clickedCell.value()
        );
    }

    return handleSelectedCell(
        clickedCell.value()
    );
}

ControllerResult NetworkController::jump(
    int x,
    int y
) {
    applyPendingUpdates();

    if (!gameState.hasSnapshot()) {
        return {
            false,
            "client_state_not_initialized"
        };
    }

    if (gameState.isGameOver()) {
        selectedCell.reset();

        return {
            false,
            Reasons::GAME_OVER
        };
    }

    const std::optional<Position> clickedCell =
        mapClickToCell(x, y);

    if (!clickedCell.has_value()) {
        selectedCell.reset();

        return {
            false,
            Reasons::CLICK_OUTSIDE
        };
    }

    return sendRequest(
        MessageType::JumpRequest,
        clickedCell.value(),
        clickedCell.value()
    );
}

// Applies all unsolicited updates currently waiting.
bool NetworkController::applyPendingUpdates() {
    bool stateWasUpdated = false;
    Message update;

    while (client.tryReceiveUpdate(update)) {
        if (gameState.applyMessage(update)) {
            stateWasUpdated = true;
        }
    }

    if (
        stateWasUpdated &&
        selectedCell.has_value()
    ) {
        const PieceSnapshot* selectedPiece =
            gameState.findPieceAt(
                selectedCell.value()
            );

        if (!isPieceAvailable(selectedPiece)) {
            selectedCell.reset();
        }
    }

    return stateWasUpdated;
}

bool NetworkController::hasSelection() const {
    return selectedCell.has_value();
}

std::optional<Position>
NetworkController::getSelectedCell() const {
    return selectedCell;
}

void NetworkController::clearSelection() {
    selectedCell.reset();
}