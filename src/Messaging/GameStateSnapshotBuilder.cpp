#include "../../include/Messaging/GameStateSnapshotBuilder.hpp"

#include "../../include/Core/Board.hpp"
#include "../../include/Core/MoveHistory.hpp"
#include "../../include/Core/Piece.hpp"
#include "../../include/Engine/GameEngine.hpp"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::string stateToString(PieceState state) {
    switch (state) {
        case PieceState::Idle:
            return "idle";

        case PieceState::Moving:
            return "moving";

        case PieceState::Airborne:
            return "airborne";

        case PieceState::Captured:
            return "captured";
    }

    return "idle";
}

std::string colorToString(PieceColor color) {
    return color == PieceColor::White
        ? "w"
        : "b";
}

std::string kindToString(PieceKind kind) {
    return std::string(
        1,
        Piece::kindToChar(kind)
    );
}

// A moving piece is still stored in its original board cell.
// For the snapshot, its current motion cell is used instead.
Position visiblePositionOf(
    const std::shared_ptr<Piece>& piece,
    const Position& boardPosition,
    const std::vector<MovingPieceInfo>& motions
) {
    for (const MovingPieceInfo& motion : motions) {
        if (motion.piece == piece) {
            return motion.currentCell;
        }
    }

    return boardPosition;
}

MoveHistorySnapshot makeHistorySnapshot(
    const MoveHistoryEntry& entry
) {
    MoveHistorySnapshot snapshot;

    snapshot.completedAtMs = entry.completedAtMs;
    snapshot.color = colorToString(entry.color);
    snapshot.pieceKind =
        kindToString(entry.pieceKind);

    snapshot.source = entry.source;
    snapshot.destination = entry.destination;

    snapshot.wasCapture = entry.wasCapture;
    snapshot.wasPromotion = entry.wasPromotion;
    snapshot.wasJump = entry.wasJump;
    snapshot.notation = entry.notation;

    return snapshot;
}

} // namespace

GameStateSnapshot GameStateSnapshotBuilder::build(
    const GameEngine& engine
) {
    GameStateSnapshot snapshot;

    snapshot.serverTimeMs =
        engine.getCurrentTimeMs();

    snapshot.gameOver =
        engine.isGameOver();

    snapshot.whiteScore =
        engine.getWhiteScore();

    snapshot.blackScore =
        engine.getBlackScore();

    const Board& board =
        engine.getBoard();

    const std::vector<MovingPieceInfo>& motions =
        engine.getMovingPieces();

    const std::vector<JumpInfo>& jumps =
        engine.getJumpingPieces();

    // Protects against accidentally adding the same piece twice.
    std::unordered_set<int> addedPieceIds;

    for (
        int row = 0;
        row < board.getHeight();
        ++row
    ) {
        for (
            int col = 0;
            col < board.getWidth();
            ++col
        ) {
            const Position boardPosition(row, col);

            const std::shared_ptr<Piece> piece =
                board.getPieceAt(boardPosition);

            if (
                piece == nullptr ||
                piece->getState() ==
                    PieceState::Captured ||
                addedPieceIds.count(
                    piece->getId()
                ) != 0
            ) {
                continue;
            }

            PieceSnapshot pieceSnapshot;

            pieceSnapshot.id =
                piece->getId();

            pieceSnapshot.token =
                piece->token();

            pieceSnapshot.position =
                visiblePositionOf(
                    piece,
                    boardPosition,
                    motions
                );

            pieceSnapshot.state =
                stateToString(
                    piece->getState()
                );

            pieceSnapshot.hasMoved =
                piece->getHasMoved();

            pieceSnapshot.remainingCooldownMs =
                piece->getRemainingCooldownMs(
                    snapshot.serverTimeMs
                );

            pieceSnapshot.totalCooldownMs =
                piece->getTotalCooldownMs();

            snapshot.pieces.push_back(
                pieceSnapshot
            );

            addedPieceIds.insert(
                piece->getId()
            );
        }
    }

    for (
        const MovingPieceInfo& motion :
        motions
    ) {
        if (motion.piece == nullptr) {
            continue;
        }

        MotionSnapshot motionSnapshot;

        motionSnapshot.pieceId =
            motion.piece->getId();

        motionSnapshot.source =
            motion.source;

        motionSnapshot.destination =
            motion.destination;

        motionSnapshot.currentCell =
            motion.currentCell;

        motionSnapshot.rowStep =
            motion.rowStep;

        motionSnapshot.colStep =
            motion.colStep;

        motionSnapshot.directMove =
            motion.directMove;

        motionSnapshot.stepDurationMs =
            motion.stepDurationMs;

        motionSnapshot.nextStepTimeMs =
            motion.nextStepTimeMs;

        motionSnapshot.order =
            motion.order;

        snapshot.motions.push_back(
            motionSnapshot
        );
    }

    for (const JumpInfo& jump : jumps) {
        if (jump.piece == nullptr) {
            continue;
        }

        JumpSnapshot jumpSnapshot;

        jumpSnapshot.pieceId =
            jump.piece->getId();

        jumpSnapshot.cell =
            jump.cell;

        jumpSnapshot.finishTimeMs =
            jump.finishTimeMs;

        snapshot.jumps.push_back(
            jumpSnapshot
        );
    }

    for (
        const MoveHistoryEntry& entry :
        engine.getWhiteMoveHistory()
    ) {
        snapshot.whiteMoveHistory.push_back(
            makeHistorySnapshot(entry)
        );
    }

    for (
        const MoveHistoryEntry& entry :
        engine.getBlackMoveHistory()
    ) {
        snapshot.blackMoveHistory.push_back(
            makeHistorySnapshot(entry)
        );
    }

    return snapshot;
}