#include "../include/GameEngine.hpp"

GameEngine::GameEngine(Board board) {
    this->board = board;
    this->gameOver = false;
}

MoveResult GameEngine::requestMove(const Position& source, const Position& destination) {
    if (gameOver) {
        return {false, Reasons::GAME_OVER};
    }

    if (realTimeArbiter.hasActiveMotion()) {
        return {false, Reasons::MOTION_IN_PROGRESS};
    }

    MoveValidation validation = ruleEngine.validateMove(board, source, destination);

    if (!validation.isValid) {
        return {false, validation.reason};
    }

    shared_ptr<Piece> piece = board.getPieceAt(source);

    if (piece == nullptr) {
        return {false, Reasons::EMPTY_SOURCE};
    }

    if (piece->getState() != PieceState::Idle) {
        return {false, Reasons::MOTION_IN_PROGRESS};
    }

    realTimeArbiter.startMotion(piece, source, destination);

    return {true, Reasons::OK};
}

MoveResult GameEngine::requestJump(const Position& cell) {
    if (gameOver) {
        return {false, Reasons::GAME_OVER};
    }

    if (!board.isInside(cell)) {
        return {false, Reasons::OUTSIDE_BOARD};
    }

    shared_ptr<Piece> piece = board.getPieceAt(cell);

    if (piece == nullptr) {
        return {false, Reasons::EMPTY_SOURCE};
    }

    // כלי בתנועה, באוויר, או נאכל — לא יכול לקפוץ
    if (piece->getState() != PieceState::Idle) {
        return {false, Reasons::CANNOT_JUMP};
    }

    realTimeArbiter.startJump(piece, cell);

    return {true, Reasons::JUMP_STARTED};
}

void GameEngine::wait(long long ms) {
    TimeEvents events = realTimeArbiter.advanceTime(ms);

    //קודם מטפלים בהגעות
    for (size_t i = 0; i < events.arrivals.size(); i++) {
        applyArrival(events.arrivals[i]);
    }

    // אחר כך מטפלים בנחיתות.
    for (size_t i = 0; i < events.jumpLandings.size(); i++) {
        applyJumpLanding(events.jumpLandings[i]);
    }
}

void GameEngine::applyArrival(const ArrivalEvent& event) {
    shared_ptr<Piece> movingPiece = board.getPieceAt(event.source);

    if (movingPiece == nullptr) {
        return;
    }

    shared_ptr<Piece> destinationPiece = board.getPieceAt(event.destination);

    if (destinationPiece != nullptr && destinationPiece->getState() == PieceState::Airborne && destinationPiece->getColor() != movingPiece->getColor()) {
        bool capturedKing = movingPiece->getKind() == PieceKind::King;

        movingPiece->setState(PieceState::Captured);
        board.removePiece(event.source);

        if (capturedKing) {
            gameOver = true;
        }

        return;
    }

    bool capturedKing = false;

    if (destinationPiece != nullptr) {
        destinationPiece->setState(PieceState::Captured);

        if (destinationPiece->getKind() == PieceKind::King) {
            capturedKing = true;
        }
    }

    board.movePiece(event.source, event.destination);

    movingPiece->setState(PieceState::Idle);

    // הכתרה: רגלי שהגיע לשורה האחרונה הופך למלכה
    if (movingPiece->getKind() == PieceKind::Pawn) {
        bool whiteReachedLastRow = movingPiece->getColor() == PieceColor::White && event.destination.getRow() == 0;
        bool blackReachedLastRow = movingPiece->getColor() == PieceColor::Black && event.destination.getRow() == board.getHeight() - 1;

        if (whiteReachedLastRow || blackReachedLastRow) {
            movingPiece->setKind(PieceKind::Queen);
        }
    }

    if (capturedKing) {
        gameOver = true;
    }
}

void GameEngine::applyJumpLanding(const JumpLandingEvent& event) {
    shared_ptr<Piece> piece = board.getPieceAt(event.cell);

    if (piece == nullptr) {
        return;
    }

    if (piece == event.piece && piece->getState() == PieceState::Airborne) {
        piece->setState(PieceState::Idle);
    }
}

const Board& GameEngine::getBoard() const {
    return board;
}

bool GameEngine::isGameOver() const {
    return gameOver;
}