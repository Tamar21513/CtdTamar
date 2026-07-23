#include "../../include/Control/Controller.hpp"
#include "../../include/Core/Config.hpp"

// Creates a controller with default board geometry.
Controller::Controller(GameEngine& engine)
    : engine(engine),
      messageBus(),
      messageHandler(engine, messageBus),
      nextSequence(1),
      selectedCell(nullopt),
      boardStartX(0),
      boardStartY(0),
      cellSizeX(Config::CELL_SIZE),
      cellSizeY(Config::CELL_SIZE) {}

// Creates a controller with explicit board geometry.
Controller::Controller(
    GameEngine& engine,
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY
) : engine(engine),
    messageBus(),
    messageHandler(engine, messageBus),
    nextSequence(1),
    selectedCell(nullopt),
    boardStartX(boardStartX),
    boardStartY(boardStartY),
    cellSizeX(cellSizeX),
    cellSizeY(cellSizeY) {}

// Maps a window pixel to a board cell.
optional<Position> Controller::mapClickToCell(
    int x,
    int y
) const {
    return BoardMapper::pixelToCell(
        x,
        y,
        engine.getBoard(),
        boardStartX,
        boardStartY,
        cellSizeX,
        cellSizeY
    );
}

// Checks whether a piece can currently be selected.
bool Controller::isPieceAvailable(
    const shared_ptr<Piece>& piece
) const {
    return piece != nullptr &&
        piece->getState() == PieceState::Idle &&
        !piece->isOnCooldown(
            engine.getCurrentTimeMs()
        );
}

// Selects the piece clicked before any selection exists.
ControllerResult Controller::selectFirstPiece(
    const Position& clickedPosition
) {
    const shared_ptr<Piece> piece =
        engine.getBoard().getPieceAt(clickedPosition);

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

// Replaces the selection with an available friendly piece.
ControllerResult Controller::selectFriendlyPiece(
    const Position& clickedPosition
) {
    const shared_ptr<Piece> piece =
        engine.getBoard().getPieceAt(clickedPosition);

    if (!isPieceAvailable(piece)) {
        selectedCell = nullopt;

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

// Sends a move request and clears the current selection.
ControllerResult Controller::requestSelectedMove(
    const Position& source,
    const Position& destination
) {
    selectedCell = nullopt;

    return sendRequest(
        MessageType::MoveRequest,
        source,
        destination
    );
}

// Sends a request through the message bus and returns the server-side response.
ControllerResult Controller::sendRequest(
    MessageType type,
    const Position& source,
    const Position& destination
) {
    Message request;

    request.type = type;
    request.sequence = nextSequence++;
    request.source = source;
    request.destination = destination;
    request.createdAtMs =
        engine.getCurrentTimeMs();

    messageBus.sendToEngine(request);

    messageHandler.processNextMessage();

    if (!messageBus.hasMessageForClient()) {
        return {
            false,
            "missing_engine_response"
        };
    }

    const Message response =
        messageBus.receiveForClient();

    if (response.sequence != request.sequence) {
        return {
            false,
            "unexpected_response_sequence"
        };
    }

    return {
        response.accepted,
        response.reason
    };
}

// Handles a click while a source cell is selected.
ControllerResult Controller::handleSelectedCell(
    const Position& clickedPosition
) {
    const Position source =
        selectedCell.value();

    if (clickedPosition == source) {
        selectedCell = nullopt;

        return sendRequest(
            MessageType::JumpRequest,
            source,
            source
        );
    }

    const shared_ptr<Piece> selectedPiece =
        engine.getBoard().getPieceAt(source);

    if (selectedPiece == nullptr) {
        selectedCell = nullopt;

        return {
            false,
            Reasons::EMPTY_SOURCE
        };
    }

    const shared_ptr<Piece> clickedPiece =
        engine.getBoard().getPieceAt(
            clickedPosition
        );

    if (
        clickedPiece != nullptr &&
        selectedPiece->getColor() ==
            clickedPiece->getColor()
    ) {
        return selectFriendlyPiece(
            clickedPosition
        );
    }

    return requestSelectedMove(
        source,
        clickedPosition
    );
}

// Handles one regular click.
ControllerResult Controller::click(
    int x,
    int y
) {
    const optional<Position> clickedCell =
        mapClickToCell(x, y);

    if (!clickedCell.has_value()) {
        const bool selectionWasCleared =
            selectedCell.has_value();

        selectedCell = nullopt;

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

// Handles one explicit jump command.
ControllerResult Controller::jump(
    int x,
    int y
) {
    const optional<Position> clickedCell =
        mapClickToCell(x, y);

    selectedCell = nullopt;

    if (!clickedCell.has_value()) {
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

// Reports whether a source cell is selected.
bool Controller::hasSelection() const {
    return selectedCell.has_value();
}

// Returns the selected source cell.
optional<Position>
Controller::getSelectedCell() const {
    return selectedCell;
}