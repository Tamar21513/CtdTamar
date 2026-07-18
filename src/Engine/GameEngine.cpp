#include "../../include/Engine/GameEngine.hpp"
#include <algorithm>

GameEngine::GameEngine(
    Board board
) : board(std::move(board)) {
    gameOver = false;

    currentTimeMs = 0;
    nextMoveOrder = 0;
}

int GameEngine::signValue(
    int value
) const {
    if (value > 0) {
        return 1;
    }

    if (value < 0) {
        return -1;
    }

    return 0;
}

MoveResult GameEngine::requestMove(
    const Position& source,
    const Position& destination
) {
    if (gameOver) {
        return {
            false,
            Reasons::GAME_OVER
        };
    }

    if (
        !board.isInside(source) ||
        !board.isInside(destination)
    ) {
        return {
            false,
            Reasons::OUTSIDE_BOARD
        };
    }

    std::shared_ptr<Piece> piece =
        board.getPieceAt(source);

    if (piece == nullptr) {
        return {
            false,
            Reasons::EMPTY_SOURCE
        };
    }

    /*
     * כלי שנע, קופץ או נאכל
     * אינו יכול להתחיל תנועה.
     */
    if (
        piece->getState() !=
        PieceState::Idle
    ) {
        return {
            false,
            Reasons::MOTION_IN_PROGRESS
        };
    }

    /*
     * כלי בזמן cooldown אינו יכול לזוז.
     */
    if (
        piece->isOnCooldown(
            currentTimeMs
        )
    ) {
        return {
            false,
            Reasons::MOTION_IN_PROGRESS
        };
    }

    MoveValidation validation =
        ruleEngine.validateMove(
            board,
            source,
            destination
        );

    if (!validation.isValid) {
        return {
            false,
            validation.reason
        };
    }

    MovingPieceInfo movingPiece;

    movingPiece.piece = piece;

    movingPiece.source =
        source;

    movingPiece.destination =
        destination;

    movingPiece.currentVirtualCell =
        source;

    movingPiece.rowStep =
        signValue(
            destination.getRow() -
            source.getRow()
        );

    movingPiece.colStep =
        signValue(
            destination.getCol() -
            source.getCol()
        );

    /*
     * סוס אינו מתקדם תא־תא בקו ישר.
     * הוא עובר ישירות לתא היעד.
     */
    movingPiece.directMove =
        piece->getKind() ==
        PieceKind::Knight;

    movingPiece.nextStepTimeMs =
        currentTimeMs +
        Config::MOVE_TIME_PER_CELL_MS;

    movingPiece.order =
        nextMoveOrder;

    nextMoveOrder++;

    movingPieces.push_back(
        movingPiece
    );

    piece->setState(
        PieceState::Moving
    );

    return {
        true,
        Reasons::OK
    };
}

MoveResult GameEngine::requestJump(
    const Position& cell
) {
    if (gameOver) {
        return {
            false,
            Reasons::GAME_OVER
        };
    }

    if (!board.isInside(cell)) {
        return {
            false,
            Reasons::OUTSIDE_BOARD
        };
    }

    std::shared_ptr<Piece> piece =
        board.getPieceAt(cell);

    if (piece == nullptr) {
        return {
            false,
            Reasons::EMPTY_SOURCE
        };
    }

    if (
        piece->getState() !=
        PieceState::Idle
    ) {
        return {
            false,
            Reasons::CANNOT_JUMP
        };
    }

    if (
        piece->isOnCooldown(
            currentTimeMs
        )
    ) {
        return {
            false,
            Reasons::CANNOT_JUMP
        };
    }

    JumpInfo jumpInfo;

    jumpInfo.piece =
        piece;

    jumpInfo.cell =
        cell;

    jumpInfo.finishTimeMs =
        currentTimeMs +
        Config::JUMP_DURATION_MS;

    jumpingPieces.push_back(
        jumpInfo
    );

    piece->setState(
        PieceState::Airborne
    );

    return {
        true,
        Reasons::JUMP_STARTED
    };
}

void GameEngine::wait(
    long long ms
) {
    if (ms < 0) {
        return;
    }

    currentTimeMs += ms;

    processTimeEvents();
}

void GameEngine::processTimeEvents() {
    while (true) {
        const int movingIndex =
            findNextMovingPieceIndex();

        const int jumpIndex =
            findNextJumpIndex();

        const bool hasMoveEvent =
            movingIndex != -1;

        const bool hasJumpEvent =
            jumpIndex != -1;

        if (
            !hasMoveEvent &&
            !hasJumpEvent
        ) {
            break;
        }

        if (
            hasMoveEvent &&
            !hasJumpEvent
        ) {
            processMovingPieceStep(
                movingIndex
            );

            continue;
        }

        if (
            !hasMoveEvent &&
            hasJumpEvent
        ) {
            processJumpLanding(
                jumpIndex
            );

            continue;
        }

        const long long moveTime =
            movingPieces[
                movingIndex
            ].nextStepTimeMs;

        const long long jumpTime =
            jumpingPieces[
                jumpIndex
            ].finishTimeMs;

        if (moveTime <= jumpTime) {
            processMovingPieceStep(
                movingIndex
            );
        } else {
            processJumpLanding(
                jumpIndex
            );
        }
    }
}

int GameEngine::findNextMovingPieceIndex()
const {
    int bestIndex = -1;

    for (
        size_t i = 0;
        i < movingPieces.size();
        i++
    ) {
        const MovingPieceInfo& movement =
            movingPieces[i];

        if (movement.piece == nullptr) {
            continue;
        }

        if (
            movement.piece->getState() !=
            PieceState::Moving
        ) {
            continue;
        }

        if (
            movement.nextStepTimeMs >
            currentTimeMs
        ) {
            continue;
        }

        if (bestIndex == -1) {
            bestIndex =
                static_cast<int>(i);

            continue;
        }

        const MovingPieceInfo& currentBest =
            movingPieces[bestIndex];

        const bool earlierTime =
            movement.nextStepTimeMs <
            currentBest.nextStepTimeMs;

        const bool sameTimeEarlierOrder =
            movement.nextStepTimeMs ==
                currentBest.nextStepTimeMs &&
            movement.order <
                currentBest.order;

        if (
            earlierTime ||
            sameTimeEarlierOrder
        ) {
            bestIndex =
                static_cast<int>(i);
        }
    }

    return bestIndex;
}

int GameEngine::findNextJumpIndex()
const {
    int bestIndex = -1;

    for (
        size_t i = 0;
        i < jumpingPieces.size();
        i++
    ) {
        const JumpInfo& jump =
            jumpingPieces[i];

        if (jump.piece == nullptr) {
            continue;
        }

        if (
            jump.piece->getState() !=
            PieceState::Airborne
        ) {
            continue;
        }

        if (
            jump.finishTimeMs >
            currentTimeMs
        ) {
            continue;
        }

        if (bestIndex == -1) {
            bestIndex =
                static_cast<int>(i);

            continue;
        }

        if (
            jump.finishTimeMs <
            jumpingPieces[
                bestIndex
            ].finishTimeMs
        ) {
            bestIndex =
                static_cast<int>(i);
        }
    }

    return bestIndex;
}

void GameEngine::processMovingPieceStep(
    int movingIndex
) {
    if (
        movingIndex < 0 ||
        movingIndex >=
            static_cast<int>(
                movingPieces.size()
            )
    ) {
        return;
    }

    /*
     * מעתיקים כדי לא להחזיק reference
     * לווקטור בזמן שמסירים ממנו איברים.
     */
    MovingPieceInfo movement =
        movingPieces[movingIndex];

    if (movement.piece == nullptr) {
        return;
    }

    if (
        movement.piece->getState() !=
        PieceState::Moving
    ) {
        return;
    }

    Position nextCell =
        movement.directMove
        ? movement.destination
        : Position(
            movement
                .currentVirtualCell
                .getRow() +
                movement.rowStep,

            movement
                .currentVirtualCell
                .getCol() +
                movement.colStep
        );

    /*
     * הגנת גבולות.
     */
    if (!board.isInside(nextCell)) {
        stopMovingPieceAt(
            movingPieces[movingIndex],
            movement.currentVirtualCell
        );

        return;
    }

    std::shared_ptr<Piece> targetPiece =
        findPieceAtRealOrVirtualCell(
            nextCell,
            movement.piece
        );

    /*
     * התא הבא פנוי.
     */
    if (targetPiece == nullptr) {
        movingPieces[movingIndex]
            .currentVirtualCell =
            nextCell;

        if (
            nextCell ==
            movement.destination
        ) {
            stopMovingPieceAt(
                movingPieces[movingIndex],
                nextCell
            );
        } else {
            movingPieces[movingIndex]
                .nextStepTimeMs +=
                Config::MOVE_TIME_PER_CELL_MS;
        }

        return;
    }

    /*
     * כלי ידידותי חוסם.
     */
    if (
        targetPiece->getColor() ==
        movement.piece->getColor()
    ) {
        stopMovingPieceAt(
            movingPieces[movingIndex],
            movement.currentVirtualCell
        );

        return;
    }

    /*
     * כלי קופץ אינו נאכל.
     * התוקף הוא שנאכל בהתנגשות.
     */
    if (
        targetPiece->getState() ==
        PieceState::Airborne
    ) {
        const bool movingKingCaptured =
            movement.piece->getKind() ==
            PieceKind::King;

        movement.piece->setState(
            PieceState::Captured
        );

        removePieceFromBoard(
            movement.piece
        );

        removeMovingPiece(
            movement.piece
        );

        if (movingKingCaptured) {
            gameOver = true;
        }

        return;
    }

    /*
     * אכילת כלי יריב.
     */
    const bool capturedKing =
        targetPiece->getKind() ==
        PieceKind::King;

    targetPiece->setState(
        PieceState::Captured
    );

    removePieceFromBoard(
        targetPiece
    );

    removeMovingPiece(
        targetPiece
    );

    const int updatedMovingIndex =
        findMovingPieceIndex(
            movement.piece
        );

    if (updatedMovingIndex != -1) {
        stopMovingPieceAt(
            movingPieces[
                updatedMovingIndex
            ],
            nextCell
        );
    }

    if (capturedKing) {
        gameOver = true;
    }
}

void GameEngine::processJumpLanding(
    int jumpIndex
) {
    if (
        jumpIndex < 0 ||
        jumpIndex >=
            static_cast<int>(
                jumpingPieces.size()
            )
    ) {
        return;
    }

    JumpInfo jumpInfo =
        jumpingPieces[jumpIndex];

    jumpingPieces.erase(
        jumpingPieces.begin() +
        jumpIndex
    );

    if (jumpInfo.piece == nullptr) {
        return;
    }

    if (
        jumpInfo.piece->getState() !=
        PieceState::Airborne
    ) {
        return;
    }

    std::shared_ptr<Piece> pieceOnBoard =
        board.getPieceAt(
            jumpInfo.cell
        );

    if (
        pieceOnBoard ==
        jumpInfo.piece
    ) {
        jumpInfo.piece->setState(
            PieceState::Idle
        );

        /*
         * לאחר קפיצה:
         * מנוחה קצרה.
         */
        jumpInfo.piece->startCooldown(
            currentTimeMs,
            Config::SHORT_COOLDOWN_MS
        );
    }
}

std::shared_ptr<Piece>
GameEngine::findPieceAtRealOrVirtualCell(
    const Position& cell,
    std::shared_ptr<Piece> exceptPiece
) const {
    /*
     * קודם בודקים כלים שנמצאים בתנועה.
     */
    for (
        const MovingPieceInfo& movement :
        movingPieces
    ) {
        std::shared_ptr<Piece> movingPiece =
            movement.piece;

        if (movingPiece == nullptr) {
            continue;
        }

        if (movingPiece == exceptPiece) {
            continue;
        }

        if (
            movingPiece->getState() !=
            PieceState::Moving
        ) {
            continue;
        }

        if (
            movement.currentVirtualCell ==
            cell
        ) {
            return movingPiece;
        }
    }

    /*
     * לאחר מכן בודקים את הלוח הלוגי.
     */
    std::shared_ptr<Piece> boardPiece =
        board.getPieceAt(cell);

    if (boardPiece == nullptr) {
        return nullptr;
    }

    if (boardPiece == exceptPiece) {
        return nullptr;
    }

    /*
     * כלי נע עדיין רשום בתא המקור הלוגי,
     * אבל אם הוא כבר עבר וירטואלית מהתא,
     * אין להתייחס אליו כאילו הוא עדיין שם.
     */
    if (
        boardPiece->getState() ==
        PieceState::Moving
    ) {
        const int movingIndex =
            findMovingPieceIndex(
                boardPiece
            );

        if (
            movingIndex != -1 &&
            movingPieces[
                movingIndex
            ].currentVirtualCell !=
                cell
        ) {
            return nullptr;
        }
    }

    return boardPiece;
}

int GameEngine::findMovingPieceIndex(
    std::shared_ptr<Piece> piece
) const {
    for (
        size_t i = 0;
        i < movingPieces.size();
        i++
    ) {
        if (
            movingPieces[i].piece ==
            piece
        ) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void GameEngine::removeMovingPiece(
    std::shared_ptr<Piece> piece
) {
    movingPieces.erase(
        std::remove_if(
            movingPieces.begin(),
            movingPieces.end(),
            [piece](
                const MovingPieceInfo& movement
            ) {
                return movement.piece == piece;
            }
        ),
        movingPieces.end()
    );
}

Position GameEngine::getLogicalCellOfPiece(
    std::shared_ptr<Piece> piece
) const {
    for (
        int row = 0;
        row < board.getHeight();
        row++
    ) {
        for (
            int col = 0;
            col < board.getWidth();
            col++
        ) {
            Position position(
                row,
                col
            );

            if (
                board.getPieceAt(position) ==
                piece
            ) {
                return position;
            }
        }
    }

    return Position(-1, -1);
}

void GameEngine::removePieceFromBoard(
    std::shared_ptr<Piece> piece
) {
    const Position logicalCell =
        getLogicalCellOfPiece(
            piece
        );

    if (board.isInside(logicalCell)) {
        board.removePiece(
            logicalCell
        );
    }
}

void GameEngine::commitPieceToCell(
    std::shared_ptr<Piece> piece,
    const Position& source,
    const Position& destination
) {
    if (piece == nullptr) {
        return;
    }

    if (
        !board.isInside(source) ||
        !board.isInside(destination)
    ) {
        return;
    }

    std::shared_ptr<Piece> pieceAtSource =
        board.getPieceAt(source);

    if (pieceAtSource == piece) {
        board.removePiece(source);
    }

    /*
     * אם נשאר כלי אויב בתא היעד,
     * מסירים אותו לפני ההצבה.
     */
    std::shared_ptr<Piece> pieceAtDestination =
        board.getPieceAt(destination);

    if (
        pieceAtDestination != nullptr &&
        pieceAtDestination != piece
    ) {
        pieceAtDestination->setState(
            PieceState::Captured
        );

        board.removePiece(
            destination
        );
    }

    board.setPieceAt(
        destination,
        piece
    );
}

void GameEngine::stopMovingPieceAt(
    MovingPieceInfo& movement,
    const Position& finalCell
) {
    std::shared_ptr<Piece> piece =
        movement.piece;

    if (piece == nullptr) {
        return;
    }

    const Position source =
        movement.source;

    int rowDistance =
        finalCell.getRow() -
        source.getRow();

    if (rowDistance < 0) {
        rowDistance =
            -rowDistance;
    }

    int colDistance =
        finalCell.getCol() -
        source.getCol();

    if (colDistance < 0) {
        colDistance =
            -colDistance;
    }

    const int moveDistance =
        rowDistance > colDistance
        ? rowDistance
        : colDistance;

    commitPieceToCell(
        piece,
        source,
        finalCell
    );

    /*
     * הקידום חייב להתבצע לאחר
     * שהכלי הונח בתא היעד.
     */
    promotePawnIfNeeded(
        piece,
        finalCell
    );

    piece->setState(
        PieceState::Idle
    );

    const long long cooldownDuration =
        moveDistance >= 4
        ? Config::LONG_COOLDOWN_MS
        : Config::SHORT_COOLDOWN_MS;

    piece->startCooldown(
        currentTimeMs,
        cooldownDuration
    );

    removeMovingPiece(
        piece
    );
}

void GameEngine::promotePawnIfNeeded(
    std::shared_ptr<Piece> piece,
    const Position& cell
) {
    if (piece == nullptr) {
        return;
    }

    if (
        piece->getKind() !=
        PieceKind::Pawn
    ) {
        return;
    }

    /*
     * לבן נע כלפי row קטן יותר.
     * לכן שורת הקידום שלו היא 0.
     */
    const bool whiteReachedLastRow =
        piece->getColor() ==
            PieceColor::White &&
        cell.getRow() == 0;

    /*
     * שחור נע כלפי row גדול יותר.
     * לכן שורת הקידום שלו היא
     * השורה האחרונה בלוח.
     */
    const bool blackReachedLastRow =
        piece->getColor() ==
            PieceColor::Black &&
        cell.getRow() ==
            board.getHeight() - 1;

    if (
        whiteReachedLastRow ||
        blackReachedLastRow
    ) {
        piece->setKind(
            PieceKind::Queen
        );
    }
}

long long GameEngine::getCurrentTimeMs()
const {
    return currentTimeMs;
}

const std::vector<MovingPieceInfo>&
GameEngine::getMovingPieces()
const {
    return movingPieces;
}

const std::vector<JumpInfo>&
GameEngine::getJumpingPieces()
const {
    return jumpingPieces;
}

const Board& GameEngine::getBoard()
const {
    return board;
}

bool GameEngine::isGameOver()
const {
    return gameOver;
}