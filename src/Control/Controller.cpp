#include "../../include/Control/Controller.hpp"

Controller::Controller(GameEngine& engine) : engine(engine) {
    selectedCell = nullopt;
}

ControllerResult Controller::click(int x, int y) {
    optional<Position> clickedCell = BoardMapper::pixelToCell(x, y, engine.getBoard());

    // קליק מחוץ ללוח
    if (!clickedCell.has_value()) {
        // אם יש בחירה — מבטלים אותה
        if (selectedCell.has_value()) {
            selectedCell = nullopt;
            return {true, Reasons::CLICK_OUTSIDE};
        }

        // אם אין בחירה — מתעלמים
        return {false, Reasons::CLICK_OUTSIDE};
    }

    Position position = clickedCell.value();

    // קליק ראשון
    if (!selectedCell.has_value()) {
        shared_ptr<Piece> piece = engine.getBoard().getPieceAt(position);

        if (piece == nullptr) {
            return {false, Reasons::EMPTY_CLICK};
        }

        selectedCell = position;
        return {true, Reasons::SELECTED};
    }

    //יש כלי נבחר
    Position source = selectedCell.value();
    
    shared_ptr<Piece> selectedPiece = engine.getBoard().getPieceAt(source);
    shared_ptr<Piece> clickedPiece = engine.getBoard().getPieceAt(position);

    // אם הקליק השני הוא על כלי ידידותי אחר — מחליפים בחירה
    if (selectedPiece != nullptr && clickedPiece != nullptr && selectedPiece->getColor() == clickedPiece->getColor()) {
        selectedCell = position;
        return {true, Reasons::SELECTED};
    }

    selectedCell = nullopt;
    MoveResult result = engine.requestMove(source, position);

    return {result.isAccepted, result.reason};
}

ControllerResult Controller::jump(int x, int y) {
    optional<Position> clickedCell = BoardMapper::pixelToCell(x, y, engine.getBoard());

    // jump לא משתמש בבחירה קיימת
    selectedCell = nullopt;

    if (!clickedCell.has_value()) {
        return {false, Reasons::CLICK_OUTSIDE};
    }

    MoveResult result = engine.requestJump(clickedCell.value());

    return {result.isAccepted, result.reason};
}

bool Controller::hasSelection() const {
    return selectedCell.has_value();
}

optional<Position> Controller::getSelectedCell() const {
    return selectedCell;
}