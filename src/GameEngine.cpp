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

    realTimeArbiter.startMotion(piece, source, destination);

    return {true, Reasons::OK};
}

void GameEngine::wait(long long ms) {
    vector<ArrivalEvent> arrivals = realTimeArbiter.advanceTime(ms);

    for (size_t i = 0; i < arrivals.size(); i++) {
        applyArrival(arrivals[i]);
    }
}

void GameEngine::applyArrival(const ArrivalEvent& event) {
    shared_ptr<Piece> movingPiece = board.getPieceAt(event.source);

    if (movingPiece == nullptr) {
        return;
    }

    shared_ptr<Piece> destinationPiece = board.getPieceAt(event.destination);

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

const Board& GameEngine::getBoard() const {
    return board;
}

bool GameEngine::isGameOver() const {
    return gameOver;
}