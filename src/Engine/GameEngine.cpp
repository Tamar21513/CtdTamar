#include "../../include/Engine/GameEngine.hpp"
#include "../../include/Core/PieceConfig.hpp"
#include "../../include/Rules/PieceRules.hpp"
#include <algorithm>
#include <cmath>

// Implements GameEngine.
GameEngine::GameEngine(Board board)
    : board(std::move(board)), gameOver(false), whiteScore(0), blackScore(0) {}

// Implements requestMove.
MoveResult GameEngine::requestMove(const Position& source, const Position& destination) {
    if (gameOver) return {false, Reasons::GAME_OVER};
    if (!board.isInside(source) || !board.isInside(destination)) {
        return {false, Reasons::OUTSIDE_BOARD};
    }

    shared_ptr<Piece> piece = board.getPieceAt(source);
    if (piece == nullptr) return {false, Reasons::EMPTY_SOURCE};
    if (piece->getState() != PieceState::Idle || piece->isOnCooldown(getCurrentTimeMs())) {
        return {false, Reasons::MOTION_IN_PROGRESS};
    }

    if (isCastlingRequest(source, destination)) {
        PendingCastle castle;
        if (!canCastle(source, destination, castle)) {
            return {false, Reasons::ILLEGAL_PIECE_MOVE};
        }

        PieceStateConfig kingConfig = PieceConfigReader::load(castle.king->token(), "move");
        long long duration = max(1LL, kingConfig.moveTimePerCellMs * 2LL);
        castle.king->markAsMoved();
        castle.rook->markAsMoved();
        realTime.startMotion(castle.king, castle.kingSource, castle.kingDestination, duration, true);
        realTime.startMotion(castle.rook, castle.rookSource, castle.rookDestination, duration, true);
        pendingCastles.push_back(castle);
        return {true, Reasons::OK};
    }

    MoveValidation validation = ruleEngine.validateMove(board, source, destination);
    if (!validation.isValid) return {false, validation.reason};

    PieceStateConfig moveConfig = PieceConfigReader::load(piece->token(), "move");
    piece->markAsMoved();
    realTime.startMotion(piece, source, destination, moveConfig.moveTimePerCellMs);
    return {true, Reasons::OK};
}

// Implements requestJump.
MoveResult GameEngine::requestJump(const Position& cell) {
    if (gameOver) return {false, Reasons::GAME_OVER};
    if (!board.isInside(cell)) return {false, Reasons::OUTSIDE_BOARD};

    shared_ptr<Piece> piece = board.getPieceAt(cell);
    if (piece == nullptr) return {false, Reasons::EMPTY_SOURCE};
    if (piece->getState() != PieceState::Idle || piece->isOnCooldown(getCurrentTimeMs())) {
        return {false, Reasons::CANNOT_JUMP};
    }

    PieceStateConfig jumpConfig = PieceConfigReader::load(piece->token(), "jump");
    realTime.startJump(piece, cell, jumpConfig.jumpDurationMs);
    return {true, Reasons::JUMP_STARTED};
}

// Handles a move or jump message and creates an acceptance or rejection response.
Message GameEngine::handleMessage(const Message& request) {
    Message response;

    response.sequence = request.sequence;
    response.source = request.source;
    response.destination = request.destination;
    response.createdAtMs = getCurrentTimeMs();

    MoveResult result;

    switch (request.type) {
        case MessageType::MoveRequest:
            result = requestMove(
                request.source,
                request.destination
            );
            break;

        case MessageType::JumpRequest:
            result = requestJump(request.source);
            break;

        default:
            response.type = MessageType::MoveRejected;
            response.accepted = false;
            response.reason = "unsupported_request_type";
            return response;
    }

    response.accepted = result.isAccepted;
    response.reason = result.reason;

    if (result.isAccepted) {
        response.type = MessageType::MoveAccepted;
    }
    else {
        response.type = MessageType::MoveRejected;
    }

    return response;
}

// Implements wait.
void GameEngine::wait(long long ms) {
    if (ms < 0) return;
    TimeEvents events = realTime.advanceTime(ms);
    while (!events.steps.empty() || !events.jumpLandings.empty()) {
        applyTimeEvents(events);
        events = realTime.advanceTime(0);
    }
}

// Implements applyTimeEvents.
void GameEngine::applyTimeEvents(const TimeEvents& events) {
    for (const StepEvent& event : events.steps) processStepEvent(event);
    for (const JumpLandingEvent& event : events.jumpLandings) processJumpLanding(event);
}

// Implements processStepEvent.
void GameEngine::processStepEvent(const StepEvent& event) {
    shared_ptr<Piece> piece = event.piece;
    if (piece == nullptr || piece->getState() != PieceState::Moving) return;
    if (completeCastleFor(event)) return;

    const Motion* motionPtr = nullptr;
    for (const Motion& motion : realTime.getActiveMotions()) {
        if (motion.piece == piece) { motionPtr = &motion; break; }
    }
    if (motionPtr == nullptr) return;
    Motion motion = *motionPtr;

    if (!board.isInside(event.to)) {
        stopMovingPieceAt(piece, motion.source, event.from, event.eventTimeMs, false);
        return;
    }

    shared_ptr<Piece> target = findPieceAtRealOrVirtualCell(event.to, piece);
    if (target == nullptr) {
        realTime.updateMotionCell(piece, event.to);
        if (event.reachedDestination) {
            stopMovingPieceAt(piece, motion.source, event.to, event.eventTimeMs, false);
        }
        return;
    }

    if (target->getColor() == piece->getColor()) {
        stopMovingPieceAt(piece, motion.source, event.from, event.eventTimeMs, false);
        return;
    }

    if (target->getState() == PieceState::Airborne) {
        PieceKind capturedKind = piece->getKind();
        bool capturedKing = capturedKind == PieceKind::King;
    
        removePieceFromBoard(piece);
        piece->setState(PieceState::Captured);
        realTime.cancelMotion(piece);
    
        // The jumping piece receives the capture points.
        addCaptureScore(target->getColor(), capturedKind);
    
        if (capturedKing)
            gameOver = true;
    
        return;
    }

    PieceKind capturedKind = target->getKind();
    bool capturedKing = capturedKind == PieceKind::King;
    target->setState(PieceState::Captured);
    removePieceFromBoard(target);
    realTime.cancelMotion(target);
    addCaptureScore(piece->getColor(), capturedKind);

    realTime.updateMotionCell(piece, event.to);
    stopMovingPieceAt(piece, motion.source, event.to, event.eventTimeMs, true);
    if (capturedKing) gameOver = true;
}

bool GameEngine::isCastlingRequest(const Position& source, const Position& destination) const {
    shared_ptr<Piece> piece = board.getPieceAt(source);
    return piece != nullptr && piece->getKind() == PieceKind::King &&
        source.getRow() == destination.getRow() &&
        abs(destination.getCol() - source.getCol()) == 2;
}

bool GameEngine::isSquareAttacked(const Position& cell, PieceColor byColor) const {
    for (int row = 0; row < board.getHeight(); ++row) {
        for (int col = 0; col < board.getWidth(); ++col) {
            Position source(row, col);
            shared_ptr<Piece> attacker = board.getPieceAt(source);
            if (attacker == nullptr || attacker->getColor() != byColor || attacker->getState() == PieceState::Captured) continue;

            if (attacker->getKind() == PieceKind::Pawn) {
                int direction = byColor == PieceColor::White ? -1 : 1;
                if (cell.getRow() - row == direction && abs(cell.getCol() - col) == 1) return true;
            } else if (attacker->getKind() == PieceKind::King) {
                if (PieceRules::canMoveLikeKing(source, cell)) return true;
            } else if (attacker->getKind() == PieceKind::Knight) {
                if (PieceRules::canMoveLikeKnight(source, cell)) return true;
            } else if (attacker->getKind() == PieceKind::Rook) {
                if (PieceRules::canMoveLikeRook(board, source, cell)) return true;
            } else if (attacker->getKind() == PieceKind::Bishop) {
                if (PieceRules::canMoveLikeBishop(board, source, cell)) return true;
            } else if (attacker->getKind() == PieceKind::Queen) {
                if (PieceRules::canMoveLikeQueen(board, source, cell)) return true;
            }
        }
    }
    return false;
}

bool GameEngine::canCastle(
    const Position& source,
    const Position& destination,
    PendingCastle& castle
) const {
    if (board.getHeight() != 8 || board.getWidth() != 8 || source.getCol() != 4) return false;
    shared_ptr<Piece> king = board.getPieceAt(source);
    if (king == nullptr || king->getKind() != PieceKind::King || king->getHasMoved()) return false;
    int homeRow = king->getColor() == PieceColor::White ? 7 : 0;
    if (source.getRow() != homeRow || destination.getRow() != homeRow) return false;

    int direction = destination.getCol() > source.getCol() ? 1 : -1;
    Position rookSource(homeRow, direction > 0 ? 7 : 0);
    Position rookDestination(homeRow, source.getCol() + direction);
    shared_ptr<Piece> rook = board.getPieceAt(rookSource);
    if (rook == nullptr || rook->getKind() != PieceKind::Rook ||
        rook->getColor() != king->getColor() || rook->getHasMoved() ||
        rook->getState() != PieceState::Idle || rook->isOnCooldown(getCurrentTimeMs())) return false;

    for (int col = source.getCol() + direction; col != rookSource.getCol(); col += direction) {
        if (!board.isEmpty(Position(homeRow, col))) return false;
    }

    PieceColor enemy = king->getColor() == PieceColor::White ? PieceColor::Black : PieceColor::White;
    Position middle(homeRow, source.getCol() + direction);
    if (isSquareAttacked(source, enemy) || isSquareAttacked(middle, enemy) ||
        isSquareAttacked(destination, enemy)) return false;

    castle = {king, rook, source, destination, rookSource, rookDestination};
    return true;
}

bool GameEngine::completeCastleFor(const StepEvent& event) {
    for (auto it = pendingCastles.begin(); it != pendingCastles.end(); ++it) {
        if (event.piece != it->king && event.piece != it->rook) continue;
        PendingCastle castle = *it;
        long long completedAt = event.eventTimeMs;

        board.removePiece(castle.kingSource);
        board.removePiece(castle.rookSource);
        board.setPieceAt(castle.kingDestination, castle.king);
        board.setPieceAt(castle.rookDestination, castle.rook);
        castle.king->setState(PieceState::Idle);
        castle.rook->setState(PieceState::Idle);
        castle.king->startCooldown(completedAt, 2000);
        castle.rook->startCooldown(completedAt, 2000);
        realTime.finishMotion(castle.king);
        realTime.finishMotion(castle.rook);

        MoveHistoryEntry entry;
        entry.completedAtMs = completedAt;
        entry.color = castle.king->getColor();
        entry.pieceKind = PieceKind::King;
        entry.source = castle.kingSource;
        entry.destination = castle.kingDestination;
        entry.notation = castle.kingDestination.getCol() > castle.kingSource.getCol() ? "O-O" : "O-O-O";
        if (entry.color == PieceColor::White) whiteMoveHistory.push_back(entry);
        else blackMoveHistory.push_back(entry);
        pendingCastles.erase(it);
        return true;
    }
    return false;
}

// Implements processJumpLanding.
void GameEngine::processJumpLanding(const JumpLandingEvent& event) {
    if (event.piece == nullptr || event.piece->getState() != PieceState::Airborne) return;
    if (board.getPieceAt(event.cell) != event.piece) return;

    event.piece->setState(PieceState::Idle);
    PieceStateConfig jumpConfig = PieceConfigReader::load(event.piece->token(), "jump");
    event.piece->startCooldown(event.eventTimeMs, jumpConfig.shortCooldownMs);

    MoveHistoryEntry entry;
    entry.completedAtMs = event.eventTimeMs;
    entry.color = event.piece->getColor();
    entry.pieceKind = event.piece->getKind();
    entry.source = event.cell;
    entry.destination = event.cell;
    entry.wasJump = true;
    entry.notation = "Jump";
    if (entry.color == PieceColor::White) whiteMoveHistory.push_back(entry);
    else blackMoveHistory.push_back(entry);
}

// Implements findPieceAtRealOrVirtualCell.
shared_ptr<Piece> GameEngine::findPieceAtRealOrVirtualCell(
    const Position& cell,
    shared_ptr<Piece> exceptPiece
) const {
    for (const Motion& motion : realTime.getActiveMotions()) {
        if (motion.piece != nullptr && motion.piece != exceptPiece &&
            motion.piece->getState() == PieceState::Moving && motion.currentCell == cell) {
            return motion.piece;
        }
    }

    shared_ptr<Piece> boardPiece = board.getPieceAt(cell);
    if (boardPiece == exceptPiece) return nullptr;
    if (boardPiece != nullptr && boardPiece->getState() == PieceState::Moving) {
        for (const Motion& motion : realTime.getActiveMotions()) {
            if (motion.piece == boardPiece && motion.currentCell != cell) return nullptr;
        }
    }
    return boardPiece;
}

// Implements getLogicalCellOfPiece.
Position GameEngine::getLogicalCellOfPiece(shared_ptr<Piece> piece) const {
    for (int row = 0; row < board.getHeight(); ++row) {
        for (int col = 0; col < board.getWidth(); ++col) {
            Position cell(row, col);
            if (board.getPieceAt(cell) == piece) return cell;
        }
    }
    return Position();
}

// Implements removePieceFromBoard.
void GameEngine::removePieceFromBoard(shared_ptr<Piece> piece) {
    Position cell = getLogicalCellOfPiece(piece);
    if (board.isInside(cell)) board.removePiece(cell);
}

// Implements commitPieceToCell.
void GameEngine::commitPieceToCell(
    shared_ptr<Piece> piece,
    const Position& source,
    const Position& destination
) {
    if (piece == nullptr || !board.isInside(destination)) return;
    if (board.getPieceAt(source) == piece) board.removePiece(source);
    board.setPieceAt(destination, piece);
}

// Implements stopMovingPieceAt.
void GameEngine::stopMovingPieceAt(
    shared_ptr<Piece> piece,
    const Position& source,
    const Position& finalCell,
    long long eventTimeMs,
    bool wasCapture
) {
    if (piece == nullptr) return;
    PieceKind originalKind = piece->getKind();
    commitPieceToCell(piece, source, finalCell);
    bool promoted = promotePawnIfNeeded(piece, finalCell);
    piece->setState(PieceState::Idle);

    int rowDistance = abs(finalCell.getRow() - source.getRow());
    int colDistance = abs(finalCell.getCol() - source.getCol());
    int distance = max(rowDistance, colDistance);
    piece->startCooldown(eventTimeMs, static_cast<long long>(distance) * 1000LL);

    realTime.finishMotion(piece);
    recordMove(piece, originalKind, source, finalCell, wasCapture, promoted, false, eventTimeMs);
}

// Implements promotePawnIfNeeded.
bool GameEngine::promotePawnIfNeeded(shared_ptr<Piece> piece, const Position& cell) {
    if (piece == nullptr || piece->getKind() != PieceKind::Pawn) return false;
    bool promoteWhite = piece->getColor() == PieceColor::White && cell.getRow() == 0;
    bool promoteBlack = piece->getColor() == PieceColor::Black && cell.getRow() == board.getHeight() - 1;
    if (!promoteWhite && !promoteBlack) return false;
    piece->setKind(PieceKind::Queen);
    return true;
}

// Implements getPieceValue.
int GameEngine::getPieceValue(PieceKind kind) const {
    switch (kind) {
        case PieceKind::Pawn: return 1;
        case PieceKind::Knight:
        case PieceKind::Bishop: return 3;
        case PieceKind::Rook: return 5;
        case PieceKind::Queen: return 9;
        case PieceKind::King: return 0;
    }
    return 0;
}

// Implements addCaptureScore.
void GameEngine::addCaptureScore(PieceColor attackerColor, PieceKind capturedKind) {
    if (attackerColor == PieceColor::White) whiteScore += getPieceValue(capturedKind);
    else blackScore += getPieceValue(capturedKind);
}

// Implements buildMoveNotation.
string GameEngine::buildMoveNotation(
    PieceKind kind,
    const Position& source,
    const Position& destination,
    bool wasCapture,
    bool wasPromotion
) const {
    string result;
    if (kind != PieceKind::Pawn) result += Piece::kindToChar(kind);
    else if (wasCapture) result += static_cast<char>('a' + source.getCol());
    if (wasCapture) result += 'x';
    result += static_cast<char>('a' + destination.getCol());
    result += to_string(board.getHeight() - destination.getRow());
    if (wasPromotion) result += "=Q";
    return result;
}

// Implements recordMove.
void GameEngine::recordMove(
    shared_ptr<Piece> piece,
    PieceKind originalKind,
    const Position& source,
    const Position& destination,
    bool wasCapture,
    bool wasPromotion,
    bool wasJump,
    long long completedAtMs
) {
    if (piece == nullptr) return;
    MoveHistoryEntry entry;
    entry.completedAtMs = completedAtMs;
    entry.color = piece->getColor();
    entry.pieceKind = originalKind;
    entry.source = source;
    entry.destination = destination;
    entry.wasCapture = wasCapture;
    entry.wasPromotion = wasPromotion;
    entry.wasJump = wasJump;
    entry.notation = buildMoveNotation(originalKind, source, destination, wasCapture, wasPromotion);
    if (entry.color == PieceColor::White) whiteMoveHistory.push_back(entry);
    else blackMoveHistory.push_back(entry);
}

// Implements getBoard.
const Board& GameEngine::getBoard() const { return board; }
// Implements getCurrentTimeMs.
long long GameEngine::getCurrentTimeMs() const { return realTime.getCurrentTimeMs(); }
// Implements getMovingPieces.
const vector<MovingPieceInfo>& GameEngine::getMovingPieces() const { return realTime.getActiveMotions(); }
// Implements getJumpingPieces.
const vector<JumpInfo>& GameEngine::getJumpingPieces() const { return realTime.getActiveJumps(); }
// Implements getRealTimeArbiter.
const RealTimeArbiter& GameEngine::getRealTimeArbiter() const { return realTime; }
// Implements getWhiteScore.
int GameEngine::getWhiteScore() const { return whiteScore; }
// Implements getBlackScore.
int GameEngine::getBlackScore() const { return blackScore; }
// Implements getWhiteMoveHistory.
const vector<MoveHistoryEntry>& GameEngine::getWhiteMoveHistory() const { return whiteMoveHistory; }
// Implements getBlackMoveHistory.
const vector<MoveHistoryEntry>& GameEngine::getBlackMoveHistory() const { return blackMoveHistory; }
// Implements isGameOver.
bool GameEngine::isGameOver() const { return gameOver; }
