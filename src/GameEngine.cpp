#include "../include/GameEngine.hpp"

GameEngine::GameEngine(Board board) {
    this->board = board;
    this->gameOver = false;
    this->currentTimeMs = 0;
    this->nextMoveOrder = 0;
}

int GameEngine::signValue(int value) const {
    if (value > 0) {
        return 1;
    }

    if (value < 0) {
        return -1;
    }

    return 0;
}

MoveResult GameEngine::requestMove(const Position& source, const Position& destination) {
    if (gameOver) {
        return {false, Reasons::GAME_OVER};
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

    MovingPieceInfo movingPiece;

    movingPiece.piece = piece;
    movingPiece.source = source;
    movingPiece.destination = destination;
    movingPiece.currentVirtualCell = source;
    movingPiece.rowStep = signValue(destination.getRow() - source.getRow());
    movingPiece.colStep = signValue(destination.getCol() - source.getCol());
    movingPiece.nextStepTimeMs = currentTimeMs + Config::MOVE_TIME_PER_CELL_MS;
    movingPiece.order = nextMoveOrder;

    nextMoveOrder++;

    movingPieces.push_back(movingPiece);

    piece->setState(PieceState::Moving);

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

    if (piece->getState() != PieceState::Idle) {
        return {false, Reasons::CANNOT_JUMP};
    }

    JumpInfo jumpInfo;

    jumpInfo.piece = piece;
    jumpInfo.cell = cell;
    jumpInfo.finishTimeMs = currentTimeMs + Config::JUMP_DURATION_MS;

    jumpingPieces.push_back(jumpInfo);

    piece->setState(PieceState::Airborne);

    return {true, Reasons::JUMP_STARTED};
}

void GameEngine::wait(long long ms) {
    currentTimeMs += ms;
    processTimeEvents();
}

void GameEngine::processTimeEvents() {
    while (true) {
        int movingIndex = findNextMovingPieceIndex();
        int jumpIndex = findNextJumpIndex();

        bool hasMoveEvent = movingIndex != -1;
        bool hasJumpEvent = jumpIndex != -1;

        if (!hasMoveEvent && !hasJumpEvent) {
            break;
        }

        if (hasMoveEvent && !hasJumpEvent) {
            processMovingPieceStep(movingIndex);
            continue;
        }

        if (!hasMoveEvent && hasJumpEvent) {
            processJumpLanding(jumpIndex);
            continue;
        }

        long long moveTime = movingPieces[movingIndex].nextStepTimeMs;
        long long jumpTime = jumpingPieces[jumpIndex].finishTimeMs;

        if (moveTime <= jumpTime) {
            processMovingPieceStep(movingIndex);
        } else {
            processJumpLanding(jumpIndex);
        }
    }
}

int GameEngine::findNextMovingPieceIndex() const {
    int bestIndex = -1;

    for (size_t i = 0; i < movingPieces.size(); i++) {
        if (movingPieces[i].piece == nullptr) {
            continue;
        }

        if (movingPieces[i].piece->getState() != PieceState::Moving) {
            continue;
        }

        if (movingPieces[i].nextStepTimeMs > currentTimeMs) {
            continue;
        }

        if (bestIndex == -1) {
            bestIndex = static_cast<int>(i);
            continue;
        }

        bool earlierTime = movingPieces[i].nextStepTimeMs < movingPieces[bestIndex].nextStepTimeMs;
        bool sameTimeButEarlierOrder = movingPieces[i].nextStepTimeMs == movingPieces[bestIndex].nextStepTimeMs && movingPieces[i].order < movingPieces[bestIndex].order;

        if (earlierTime || sameTimeButEarlierOrder) {
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

int GameEngine::findNextJumpIndex() const {
    int bestIndex = -1;

    for (size_t i = 0; i < jumpingPieces.size(); i++) {
        if (jumpingPieces[i].piece == nullptr) {
            continue;
        }

        if (jumpingPieces[i].piece->getState() != PieceState::Airborne) {
            continue;
        }

        if (jumpingPieces[i].finishTimeMs > currentTimeMs) {
            continue;
        }

        if (bestIndex == -1) {
            bestIndex = static_cast<int>(i);
            continue;
        }

        if (jumpingPieces[i].finishTimeMs < jumpingPieces[bestIndex].finishTimeMs) {
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

void GameEngine::processMovingPieceStep(int movingIndex) {
    if (movingIndex < 0 || movingIndex >= static_cast<int>(movingPieces.size())) {
        return;
    }

    MovingPieceInfo& movingPiece = movingPieces[movingIndex];

    if (movingPiece.piece == nullptr) {
        return;
    }

    if (movingPiece.piece->getState() != PieceState::Moving) {
        return;
    }

    Position nextCell(
        movingPiece.currentVirtualCell.getRow() + movingPiece.rowStep,
        movingPiece.currentVirtualCell.getCol() + movingPiece.colStep
    );

    shared_ptr<Piece> targetPiece = findPieceAtRealOrVirtualCell(nextCell, movingPiece.piece);

    if (targetPiece == nullptr) {
        movingPiece.currentVirtualCell = nextCell;

        if (nextCell == movingPiece.destination) {
            stopMovingPieceAt(movingPiece, nextCell);
        } else {
            movingPiece.nextStepTimeMs += Config::MOVE_TIME_PER_CELL_MS;
        }

        return;
    }

    if (targetPiece->getColor() == movingPiece.piece->getColor()) {
        stopMovingPieceAt(movingPiece, movingPiece.currentVirtualCell);
        return;
    }

    if (targetPiece->getState() == PieceState::Airborne) {
        bool movingKingWasCaptured = movingPiece.piece->getKind() == PieceKind::King;

        movingPiece.piece->setState(PieceState::Captured);
        removePieceFromBoard(movingPiece.piece);
        removeMovingPiece(movingPiece.piece);

        if (movingKingWasCaptured) {
            gameOver = true;
        }

        return;
    }

    bool capturedKing = targetPiece->getKind() == PieceKind::King;

    targetPiece->setState(PieceState::Captured);
    removePieceFromBoard(targetPiece);
    removeMovingPiece(targetPiece);

    stopMovingPieceAt(movingPiece, nextCell);

    if (capturedKing) {
        gameOver = true;
    }
}

void GameEngine::processJumpLanding(int jumpIndex) {
    if (jumpIndex < 0 || jumpIndex >= static_cast<int>(jumpingPieces.size())) {
        return;
    }

    JumpInfo jumpInfo = jumpingPieces[jumpIndex];

    jumpingPieces.erase(jumpingPieces.begin() + jumpIndex);

    if (jumpInfo.piece == nullptr) {
        return;
    }

    if (jumpInfo.piece->getState() != PieceState::Airborne) {
        return;
    }

    shared_ptr<Piece> pieceOnBoard = board.getPieceAt(jumpInfo.cell);

    if (pieceOnBoard == jumpInfo.piece) {
        jumpInfo.piece->setState(PieceState::Idle);
    }
}

shared_ptr<Piece> GameEngine::findPieceAtRealOrVirtualCell(const Position& cell, shared_ptr<Piece> exceptPiece) const {
    for (size_t i = 0; i < movingPieces.size(); i++) {
        shared_ptr<Piece> movingPiece = movingPieces[i].piece;

        if (movingPiece == nullptr) {
            continue;
        }

        if (movingPiece == exceptPiece) {
            continue;
        }

        if (movingPiece->getState() != PieceState::Moving) {
            continue;
        }

        if (movingPieces[i].currentVirtualCell == cell) {
            return movingPiece;
        }
    }

    shared_ptr<Piece> boardPiece = board.getPieceAt(cell);

    if (boardPiece == nullptr) {
        return nullptr;
    }

    if (boardPiece == exceptPiece) {
        return nullptr;
    }

    if (boardPiece->getState() == PieceState::Moving) {
        int movingIndex = findMovingPieceIndex(boardPiece);

        if (movingIndex != -1 && movingPieces[movingIndex].currentVirtualCell != cell) {
            return nullptr;
        }
    }

    return boardPiece;
}

int GameEngine::findMovingPieceIndex(shared_ptr<Piece> piece) const {
    for (size_t i = 0; i < movingPieces.size(); i++) {
        if (movingPieces[i].piece == piece) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void GameEngine::removeMovingPiece(shared_ptr<Piece> piece) {
    vector<MovingPieceInfo> remaining;

    for (size_t i = 0; i < movingPieces.size(); i++) {
        if (movingPieces[i].piece != piece) {
            remaining.push_back(movingPieces[i]);
        }
    }

    movingPieces = remaining;
}

Position GameEngine::getLogicalCellOfPiece(shared_ptr<Piece> piece) const {
    for (int row = 0; row < board.getHeight(); row++) {
        for (int col = 0; col < board.getWidth(); col++) {
            Position position(row, col);

            if (board.getPieceAt(position) == piece) {
                return position;
            }
        }
    }

    return Position(-1, -1);
}

void GameEngine::removePieceFromBoard(shared_ptr<Piece> piece) {
    Position logicalCell = getLogicalCellOfPiece(piece);

    if (board.isInside(logicalCell)) {
        board.removePiece(logicalCell);
    }
}

void GameEngine::commitPieceToCell(shared_ptr<Piece> piece, const Position& source, const Position& destination) {
    if (piece == nullptr) {
        return;
    }

    if (!board.isInside(source) || !board.isInside(destination)) {
        return;
    }
    shared_ptr<Piece> pieceAtSource = board.getPieceAt(source);
    if (pieceAtSource == piece) {
        board.removePiece(source);
    }

    board.setPieceAt(destination, piece);
}

void GameEngine::stopMovingPieceAt(MovingPieceInfo& movingPiece, const Position& finalCell) {
    shared_ptr<Piece> piece = movingPiece.piece;

    if (piece == nullptr) {
        return;
    }

    Position source = movingPiece.source;

    commitPieceToCell(piece, source, finalCell);

    piece->setState(PieceState::Idle);

    promotePawnIfNeeded(piece, finalCell);

    removeMovingPiece(piece);
}

void GameEngine::promotePawnIfNeeded(shared_ptr<Piece> piece, const Position& cell) {
    if (piece == nullptr) {
        return;
    }

    if (piece->getKind() != PieceKind::Pawn) {
        return;
    }

    bool whiteReachedLastRow =
        piece->getColor() == PieceColor::White &&
        cell.getRow() == 0;

    bool blackReachedLastRow =
        piece->getColor() == PieceColor::Black &&
        cell.getRow() == board.getHeight() - 1;

    if (whiteReachedLastRow || blackReachedLastRow) {
        piece->setKind(PieceKind::Queen);
    }
}

const Board& GameEngine::getBoard() const {
    return board;
}

bool GameEngine::isGameOver() const {
    return gameOver;
}