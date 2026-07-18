#include "../../include/Graphics/VisualSnapshotBuilder.hpp"
#include "../../include/Core/Config.hpp"

#include <algorithm>
#include <memory>
#include <vector>

VisualSnapshotBuilder::VisualSnapshotBuilder(
    int boardStartX,
    int boardStartY,
    int cellSizeX,
    int cellSizeY,
    int spriteSize,
    const AnimationLibrary& animationLibrary
) : animationLibrary(animationLibrary) {
    this->boardStartX = boardStartX;
    this->boardStartY = boardStartY;

    this->cellSizeX = cellSizeX;
    this->cellSizeY = cellSizeY;

    this->spriteSize = spriteSize;
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

const MovingPieceInfo*
VisualSnapshotBuilder::findMovement(
    const GameEngine& engine,
    int pieceId
) const {
    const std::vector<MovingPieceInfo>& movements =
        engine.getMovingPieces();

    for (const MovingPieceInfo& movement : movements) {
        if (
            movement.piece != nullptr &&
            movement.piece->getId() == pieceId
        ) {
            return &movement;
        }
    }

    return nullptr;
}

const JumpInfo*
VisualSnapshotBuilder::findJump(
    const GameEngine& engine,
    int pieceId
) const {
    const std::vector<JumpInfo>& jumps =
        engine.getJumpingPieces();

    for (const JumpInfo& jump : jumps) {
        if (
            jump.piece != nullptr &&
            jump.piece->getId() == pieceId
        ) {
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
            [&visiblePieceIds](const CachedPiece& cached) {
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

std::vector<VisualPiece>
VisualSnapshotBuilder::build(
    const GameEngine& engine,
    long long deltaMs
) {
    const Board& board =
        engine.getBoard();

    std::vector<int> visiblePieceIds;

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
            Position boardPosition(
                row,
                col
            );

            std::shared_ptr<Piece> piece =
                board.getPieceAt(
                    boardPosition
                );

            if (piece == nullptr) {
                continue;
            }

            if (
                piece->getState() ==
                PieceState::Captured
            ) {
                continue;
            }

            const int pieceId =
                piece->getId();

            visiblePieceIds.push_back(
                pieceId
            );

            const long long currentTimeMs =
                engine.getCurrentTimeMs();

            const long long remainingCooldownMs =
                piece->getRemainingCooldownMs(
                    currentTimeMs
                );

            const long long totalCooldownMs =
                piece->getTotalCooldownMs();

            VisualState visualState =
                visualStateMachine.chooseState(
                    piece->getState(),
                    remainingCooldownMs,
                    totalCooldownMs
                );

            const double cooldownRatio =
                piece->getCooldownRatio(
                    currentTimeMs
                );

            double pixelX =
                cellCenterX(col);

            double pixelY =
                cellCenterY(row);

            int visualRow = row;
            int visualCol = col;

            /*
             * תנועה רגילה.
             */
            const MovingPieceInfo* movement =
                findMovement(
                    engine,
                    pieceId
                );

            if (movement != nullptr) {
                visualRow =
                    movement
                        ->currentVirtualCell
                        .getRow();

                visualCol =
                    movement
                        ->currentVirtualCell
                        .getCol();

                Position nextCell =
                    movement->directMove
                    ? movement->destination
                    : Position(
                        visualRow +
                            movement->rowStep,
                        visualCol +
                            movement->colStep
                    );

                const long long segmentStartTimeMs =
                    movement->nextStepTimeMs -
                    Config::MOVE_TIME_PER_CELL_MS;

                double progress =
                    static_cast<double>(
                        currentTimeMs -
                        segmentStartTimeMs
                    ) /
                    static_cast<double>(
                        Config::MOVE_TIME_PER_CELL_MS
                    );

                if (progress < 0.0) {
                    progress = 0.0;
                }

                if (progress > 1.0) {
                    progress = 1.0;
                }

                const double startX =
                    cellCenterX(
                        visualCol
                    );

                const double startY =
                    cellCenterY(
                        visualRow
                    );

                const double endX =
                    cellCenterX(
                        nextCell.getCol()
                    );

                const double endY =
                    cellCenterY(
                        nextCell.getRow()
                    );

                pixelX =
                    startX +
                    (endX - startX) *
                    progress;

                pixelY =
                    startY +
                    (endY - startY) *
                    progress;
            }

            /*
             * קפיצה באותו תא.
             *
             * הקפיצה משנה רק את מיקום Y
             * של התצוגה. היא אינה משנה
             * את התא הלוגי של הכלי.
             */
            const JumpInfo* jump =
                findJump(
                    engine,
                    pieceId
                );

            if (jump != nullptr) {
                const long long jumpStartTimeMs =
                    jump->finishTimeMs -
                    Config::JUMP_DURATION_MS;

                double jumpProgress =
                    static_cast<double>(
                        currentTimeMs -
                        jumpStartTimeMs
                    ) /
                    static_cast<double>(
                        Config::JUMP_DURATION_MS
                    );

                if (jumpProgress < 0.0) {
                    jumpProgress = 0.0;
                }

                if (jumpProgress > 1.0) {
                    jumpProgress = 1.0;
                }

                /*
                 * פרבולה:
                 *
                 * התחלה: 0
                 * אמצע: 1
                 * סיום: 0
                 */
                const double jumpArc =
                    4.0 *
                    jumpProgress *
                    (1.0 - jumpProgress);

                pixelY -=
                    Config::JUMP_HEIGHT_PIXELS *
                    jumpArc;
            }

            CachedPiece* cached =
                findCachedPiece(
                    pieceId
                );

            if (cached == nullptr) {
                cachedPieces.push_back({
                    pieceId,
                    AnimatedPiece(
                        piece->token(),
                        visualState,
                        visualRow,
                        visualCol,
                        pixelX,
                        pixelY,
                        cooldownRatio,
                        animationLibrary
                    )
                });

                cached =
                    &cachedPieces.back();
            }

            /*
             * קריטי עבור קידום רגלי:
             *
             * pieceId נשאר זהה,
             * אבל token משתנה:
             *
             * wP -> wQ
             * bP -> bQ
             *
             * בלי השורה הזו ה־cache הגרפי
             * ממשיך לצייר את הרגלי.
             */
            cached->animatedPiece.setPieceCode(
                piece->token()
            );

            cached->animatedPiece.setState(
                visualState
            );

            cached->animatedPiece.setBoardCell(
                visualRow,
                visualCol
            );

            cached->animatedPiece.setPixelPosition(
                pixelX,
                pixelY
            );

            cached->animatedPiece.setCooldownRatio(
                cooldownRatio
            );

            cached->animatedPiece.update(
                deltaMs
            );
        }
    }

    removeMissingPieces(
        visiblePieceIds
    );

    std::vector<VisualPiece> snapshot;

    snapshot.reserve(
        cachedPieces.size()
    );

    for (
        const CachedPiece& cached :
        cachedPieces
    ) {
        VisualPiece visualPiece =
            cached
                .animatedPiece
                .toVisualPiece();

        visualPiece.pieceId =
            cached.pieceId;

        snapshot.push_back(
            visualPiece
        );
    }

    return snapshot;
}