#include "../../include/Graphics/VisualSnapshotBuilder.hpp"
#include "../../include/Core/Config.hpp"

#include <algorithm>
#include <vector>

namespace {

double clampProgress(double progress) {
    return std::max(
        0.0,
        std::min(1.0, progress)
    );
}

PieceState pieceStateFromString(
    const std::string& state
) {
    if (state == "moving") {
        return PieceState::Moving;
    }

    if (state == "airborne") {
        return PieceState::Airborne;
    }

    if (state == "captured") {
        return PieceState::Captured;
    }

    return PieceState::Idle;
}

double calculateCooldownRatio(
    const PieceSnapshot& piece
) {
    if (
        piece.totalCooldownMs <= 0 ||
        piece.remainingCooldownMs <= 0
    ) {
        return 0.0;
    }

    return clampProgress(
        static_cast<double>(
            piece.remainingCooldownMs
        ) /
        static_cast<double>(
            piece.totalCooldownMs
        )
    );
}

} // namespace

VisualSnapshotBuilder::VisualSnapshotBuilder(
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY,
    int spriteSize,
    const AnimationLibrary& animationLibrary
) : boardStartX(boardStartX),
    boardStartY(boardStartY),
    cellSizeX(cellSizeX),
    cellSizeY(cellSizeY),
    spriteSize(spriteSize),
    animationLibrary(animationLibrary) {
}

double VisualSnapshotBuilder::cellCenterX(
    int col
) const {
    return
        boardStartX +
        col * cellSizeX +
        cellSizeX / 2.0;
}

double VisualSnapshotBuilder::cellCenterY(
    int row
) const {
    return
        boardStartY +
        row * cellSizeY +
        cellSizeY / 2.0;
}

VisualSnapshotBuilder::CachedPiece*
VisualSnapshotBuilder::findCachedPiece(
    int pieceId
) {
    for (CachedPiece& cached : cachedPieces) {
        if (cached.pieceId == pieceId) {
            return &cached;
        }
    }

    return nullptr;
}

const MotionSnapshot*
VisualSnapshotBuilder::findMovement(
    const GameStateSnapshot& snapshot,
    int pieceId
) const {
    for (
        const MotionSnapshot& movement :
        snapshot.motions
    ) {
        if (movement.pieceId == pieceId) {
            return &movement;
        }
    }

    return nullptr;
}

const JumpSnapshot*
VisualSnapshotBuilder::findJump(
    const GameStateSnapshot& snapshot,
    int pieceId
) const {
    for (
        const JumpSnapshot& jump :
        snapshot.jumps
    ) {
        if (jump.pieceId == pieceId) {
            return &jump;
        }
    }

    return nullptr;
}

void VisualSnapshotBuilder::removeMissingPieces(
    const std::vector<int>& visiblePieceIds
) {
    cachedPieces.erase(
        std::remove_if(
            cachedPieces.begin(),
            cachedPieces.end(),
            [&visiblePieceIds](
                const CachedPiece& cached
            ) {
                return std::find(
                    visiblePieceIds.begin(),
                    visiblePieceIds.end(),
                    cached.pieceId
                ) == visiblePieceIds.end();
            }
        ),
        cachedPieces.end()
    );
}

void VisualSnapshotBuilder::applyMovement(
    VisualPlacement& placement,
    const MotionSnapshot& movement,
    long long currentTimeMs
) const {
    placement.row =
        movement.currentCell.getRow();

    placement.col =
        movement.currentCell.getCol();

    placement.pixelX =
        cellCenterX(placement.col);

    placement.pixelY =
        cellCenterY(placement.row);

    if (movement.stepDurationMs <= 0) {
        return;
    }

    const Position nextCell =
        movement.directMove
            ? movement.destination
            : Position(
                placement.row +
                    movement.rowStep,
                placement.col +
                    movement.colStep
            );

    const long long segmentStartTimeMs =
        movement.nextStepTimeMs -
        movement.stepDurationMs;

    const double progress =
        clampProgress(
            static_cast<double>(
                currentTimeMs -
                segmentStartTimeMs
            ) /
            static_cast<double>(
                movement.stepDurationMs
            )
        );

    const double startX =
        cellCenterX(placement.col);

    const double startY =
        cellCenterY(placement.row);

    placement.pixelX =
        startX +
        (
            cellCenterX(nextCell.getCol()) -
            startX
        ) * progress;

    placement.pixelY =
        startY +
        (
            cellCenterY(nextCell.getRow()) -
            startY
        ) * progress;
}

void VisualSnapshotBuilder::applyJump(
    VisualPlacement& placement,
    const JumpSnapshot& jump,
    long long currentTimeMs
) const {
    const long long startTimeMs =
        jump.finishTimeMs -
        Config::JUMP_DURATION_MS;

    const double progress =
        clampProgress(
            static_cast<double>(
                currentTimeMs - startTimeMs
            ) /
            static_cast<double>(
                Config::JUMP_DURATION_MS
            )
        );

    const double jumpArc =
        4.0 * progress * (1.0 - progress);

    placement.pixelY -=
        Config::JUMP_HEIGHT_PIXELS *
        jumpArc;
}

VisualSnapshotBuilder::VisualPlacement
VisualSnapshotBuilder::calculatePlacement(
    const GameStateSnapshot& snapshot,
    const PieceSnapshot& piece
) const {
    VisualPlacement placement{
        piece.position.getRow(),
        piece.position.getCol(),
        cellCenterX(piece.position.getCol()),
        cellCenterY(piece.position.getRow())
    };

    const MotionSnapshot* movement =
        findMovement(snapshot, piece.id);

    if (movement != nullptr) {
        applyMovement(
            placement,
            *movement,
            snapshot.serverTimeMs
        );
    }

    const JumpSnapshot* jump =
        findJump(snapshot, piece.id);

    if (jump != nullptr) {
        applyJump(
            placement,
            *jump,
            snapshot.serverTimeMs
        );
    }

    return placement;
}

VisualSnapshotBuilder::CachedPiece&
VisualSnapshotBuilder::getOrCreateCachedPiece(
    const PieceSnapshot& piece,
    VisualState visualState,
    const VisualPlacement& placement,
    double cooldownRatio
) {
    CachedPiece* cached =
        findCachedPiece(piece.id);

    if (cached == nullptr) {
        cachedPieces.push_back({
            piece.id,
            AnimatedPiece(
                piece.token,
                visualState,
                placement.row,
                placement.col,
                placement.pixelX,
                placement.pixelY,
                cooldownRatio,
                animationLibrary
            )
        });

        cached = &cachedPieces.back();
    }

    return *cached;
}

void VisualSnapshotBuilder::updateCachedPiece(
    CachedPiece& cached,
    const PieceSnapshot& piece,
    VisualState visualState,
    const VisualPlacement& placement,
    double cooldownRatio,
    long long deltaMs
) {
    cached.animatedPiece.setPieceCode(
        piece.token
    );

    cached.animatedPiece.setState(
        visualState
    );

    cached.animatedPiece.setBoardCell(
        placement.row,
        placement.col
    );

    cached.animatedPiece.setPixelPosition(
        placement.pixelX,
        placement.pixelY
    );

    cached.animatedPiece.setCooldownRatio(
        cooldownRatio
    );

    cached.animatedPiece.update(deltaMs);
}

std::vector<VisualPiece>
VisualSnapshotBuilder::createSnapshot() const {
    std::vector<VisualPiece> snapshot;

    snapshot.reserve(
        cachedPieces.size()
    );

    for (
        const CachedPiece& cached :
        cachedPieces
    ) {
        VisualPiece visualPiece =
            cached.animatedPiece.toVisualPiece();

        visualPiece.pieceId =
            cached.pieceId;

        snapshot.push_back(
            visualPiece
        );
    }

    return snapshot;
}

std::vector<VisualPiece>
VisualSnapshotBuilder::build(
    const GameStateSnapshot& snapshot,
    long long deltaMs
) {
    std::vector<int> visiblePieceIds;

    visiblePieceIds.reserve(
        snapshot.pieces.size()
    );

    for (
        const PieceSnapshot& piece :
        snapshot.pieces
    ) {
        if (piece.state == "captured") {
            continue;
        }

        visiblePieceIds.push_back(
            piece.id
        );

        const VisualState visualState =
            visualStateMachine.chooseState(
                pieceStateFromString(piece.state),
                piece.remainingCooldownMs,
                piece.totalCooldownMs
            );

        const double cooldownRatio =
            calculateCooldownRatio(piece);

        const VisualPlacement placement =
            calculatePlacement(
                snapshot,
                piece
            );

        CachedPiece& cached =
            getOrCreateCachedPiece(
                piece,
                visualState,
                placement,
                cooldownRatio
            );

        updateCachedPiece(
            cached,
            piece,
            visualState,
            placement,
            cooldownRatio,
            deltaMs
        );
    }

    removeMissingPieces(
        visiblePieceIds
    );

    return createSnapshot();
}