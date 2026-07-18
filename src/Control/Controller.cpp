#include "../../include/Control/Controller.hpp"
#include "../../include/Core/Config.hpp"

Controller::Controller(
    GameEngine& engine
) : engine(engine) {
    selectedCell = nullopt;

    boardStartX = 0;
    boardStartY = 0;

    cellSizeX =
        Config::CELL_SIZE;

    cellSizeY =
        Config::CELL_SIZE;
}

Controller::Controller(
    GameEngine& engine,
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY
) : engine(engine) {
    selectedCell = nullopt;

    this->boardStartX =
        boardStartX;

    this->boardStartY =
        boardStartY;

    this->cellSizeX =
        cellSizeX;

    this->cellSizeY =
        cellSizeY;
}

optional<Position>
Controller::mapClickToCell(
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

ControllerResult Controller::click(
    int x,
    int y
) {
    optional<Position> clickedCell =
        mapClickToCell(
            x,
            y
        );

    if (!clickedCell.has_value()) {
        if (selectedCell.has_value()) {
            selectedCell = nullopt;

            return {
                true,
                Reasons::CLICK_OUTSIDE
            };
        }

        return {
            false,
            Reasons::CLICK_OUTSIDE
        };
    }

    Position clickedPosition =
        clickedCell.value();

    /*
     * לחיצה ראשונה.
     */
    if (!selectedCell.has_value()) {
        shared_ptr<Piece> clickedPiece =
            engine.getBoard().getPieceAt(
                clickedPosition
            );

        if (clickedPiece == nullptr) {
            return {
                false,
                Reasons::EMPTY_CLICK
            };
        }

        /*
         * כלי שנע או קופץ אינו ניתן לבחירה.
         */
        if (
            clickedPiece->getState() !=
            PieceState::Idle
        ) {
            return {
                false,
                Reasons::MOTION_IN_PROGRESS
            };
        }

        /*
         * כלי במנוחה אינו ניתן לבחירה.
         *
         * לכן גם העותק השקוף לא יופיע
         * ולא ינוע אחרי העכבר.
         */
        if (
            clickedPiece->isOnCooldown(
                engine.getCurrentTimeMs()
            )
        ) {
            return {
                false,
                Reasons::MOTION_IN_PROGRESS
            };
        }

        selectedCell =
            clickedPosition;

        return {
            true,
            Reasons::SELECTED
        };
    }

    Position source =
        selectedCell.value();

    /*
     * לחיצה שנייה על אותו תא:
     * שולחים בקשת קפיצה למנוע.
     */
    if (clickedPosition == source) {
        selectedCell = nullopt;

        MoveResult jumpResult =
            engine.requestJump(source);

        return {
            jumpResult.isAccepted,
            jumpResult.reason
        };
    }

    shared_ptr<Piece> selectedPiece =
        engine.getBoard().getPieceAt(
            source
        );

    shared_ptr<Piece> clickedPiece =
        engine.getBoard().getPieceAt(
            clickedPosition
        );

    if (selectedPiece == nullptr) {
        selectedCell = nullopt;

        return {
            false,
            Reasons::EMPTY_SOURCE
        };
    }

    /*
     * לחיצה על כלי ידידותי אחר
     * מחליפה בחירה רק אם הוא זמין.
     */
    if (
        clickedPiece != nullptr &&
        selectedPiece->getColor() ==
            clickedPiece->getColor()
    ) {
        if (
            clickedPiece->getState() !=
            PieceState::Idle
        ) {
            selectedCell = nullopt;

            return {
                false,
                Reasons::MOTION_IN_PROGRESS
            };
        }

        if (
            clickedPiece->isOnCooldown(
                engine.getCurrentTimeMs()
            )
        ) {
            selectedCell = nullopt;

            return {
                false,
                Reasons::MOTION_IN_PROGRESS
            };
        }

        selectedCell =
            clickedPosition;

        return {
            true,
            Reasons::SELECTED
        };
    }

    selectedCell = nullopt;

    MoveResult moveResult =
        engine.requestMove(
            source,
            clickedPosition
        );

    return {
        moveResult.isAccepted,
        moveResult.reason
    };
}

ControllerResult Controller::jump(
    int x,
    int y
) {
    optional<Position> clickedCell =
        mapClickToCell(
            x,
            y
        );

    selectedCell = nullopt;

    if (!clickedCell.has_value()) {
        return {
            false,
            Reasons::CLICK_OUTSIDE
        };
    }

    MoveResult result =
        engine.requestJump(
            clickedCell.value()
        );

    return {
        result.isAccepted,
        result.reason
    };
}

bool Controller::hasSelection() const {
    return selectedCell.has_value();
}

optional<Position>
Controller::getSelectedCell() const {
    return selectedCell;
}