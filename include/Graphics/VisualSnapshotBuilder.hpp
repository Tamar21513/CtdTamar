#pragma once

#include <vector>

#include "AnimatedPiece.hpp"
#include "AnimationLibrary.hpp"
#include "Renderer.hpp"
#include "VisualStateMachine.hpp"

#include "../Messaging/GameStateSnapshot.hpp"

class VisualSnapshotBuilder {
private:
    struct CachedPiece {
        int pieceId;
        AnimatedPiece animatedPiece;
    };

    struct VisualPlacement {
        int row;
        int col;
        double pixelX;
        double pixelY;
    };

    int boardStartX;
    int boardStartY;

    int cellSizeX;
    int cellSizeY;

    int spriteSize;

    const AnimationLibrary&
        animationLibrary;

    VisualStateMachine
        visualStateMachine;

    std::vector<CachedPiece>
        cachedPieces;

    double cellCenterX(int col) const;
    double cellCenterY(int row) const;

    CachedPiece* findCachedPiece(
        int pieceId
    );

    const MotionSnapshot* findMovement(
        const GameStateSnapshot& snapshot,
        int pieceId
    ) const;

    const JumpSnapshot* findJump(
        const GameStateSnapshot& snapshot,
        int pieceId
    ) const;

    void removeMissingPieces(
        const std::vector<int>& visiblePieceIds
    );

    VisualPlacement calculatePlacement(
        const GameStateSnapshot& snapshot,
        const PieceSnapshot& piece
    ) const;

    void applyMovement(
        VisualPlacement& placement,
        const MotionSnapshot& movement,
        long long currentTimeMs
    ) const;

    void applyJump(
        VisualPlacement& placement,
        const JumpSnapshot& jump,
        long long currentTimeMs
    ) const;

    CachedPiece& getOrCreateCachedPiece(
        const PieceSnapshot& piece,
        VisualState visualState,
        const VisualPlacement& placement,
        double cooldownRatio
    );

    void updateCachedPiece(
        CachedPiece& cached,
        const PieceSnapshot& piece,
        VisualState visualState,
        const VisualPlacement& placement,
        double cooldownRatio,
        long long deltaMs
    );

    std::vector<VisualPiece>
    createSnapshot() const;

public:
    VisualSnapshotBuilder(
        int boardStartX,
        int boardStartY,
        int cellSizeX,
        int cellSizeY,
        int spriteSize,
        const AnimationLibrary&
            animationLibrary
    );

    std::vector<VisualPiece> build(
        const GameStateSnapshot& snapshot,
        long long deltaMs
    );
};